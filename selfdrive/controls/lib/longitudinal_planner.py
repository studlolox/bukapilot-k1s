#!/usr/bin/env python3
import math
import numpy as np
from common.numpy_fast import interp

import cereal.messaging as messaging
from common.filter_simple import FirstOrderFilter
from common.realtime import DT_MDL
from selfdrive.modeld.constants import T_IDXS
from selfdrive.config import Conversions as CV
from selfdrive.controls.lib.longcontrol import LongCtrlState
from selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc import LongitudinalMpc
from selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc import T_IDXS as T_IDXS_MPC
from selfdrive.controls.lib.drive_helpers import V_CRUISE_MAX, CONTROL_N
from selfdrive.swaglog import cloudlog

LON_MPC_STEP = 0.2  # first step is 0.2s
AWARENESS_DECEL = -0.2  # car smoothly decel at .2m/s^2 when user is distracted
A_CRUISE_MIN = -1.2
A_CRUISE_MAX_VALS = [1.2, 1.0, 0.8, 0.6]
A_CRUISE_MAX_BP = [0., 15., 25., 40.]

# Lookup table for turns
_A_TOTAL_MAX_V = [2.7, 3.7]
_A_TOTAL_MAX_BP = [20., 40.]

# VTSC (Vision Turn Speed Controller) constants
VTSC_A_LAT_MAX = 2.0  # m/s^2 (~0.2G comfortable cornering limit)
VTSC_A_DECEL = 1.3    # m/s^2 comfortable deceleration rate ahead of turn


def eval_vtsc(v_ego, v_cruise, curvatures, t_idxs, vtsc_mode, e2e_active):
  """
  Calculates a comfortable curve entry speed based on upcoming path curvatures.
  vtsc_mode: 0 = Off, 1 = E2E Only, 2 = Always On
  """
  if vtsc_mode == 0 or (vtsc_mode == 1 and not e2e_active):
    return v_cruise

  if curvatures is None or len(curvatures) == 0:
    return v_cruise

  try:
    curv_arr = np.abs(np.array(curvatures))
    if len(curv_arr) == 0 or np.isnan(curv_arr).any():
      return v_cruise

    # Avoid div by 0
    curv_arr = np.maximum(curv_arr, 1e-4)

    # v_turn = sqrt(a_lat / curvature)
    v_turns = np.sqrt(VTSC_A_LAT_MAX / curv_arr)

    # Kinematic lookahead: target speed at current position factoring in decel time
    if t_idxs is not None and len(t_idxs) >= len(curv_arr):
      t_arr = np.array(t_idxs[:len(curv_arr)])
      v_targets = v_turns + VTSC_A_DECEL * t_arr
    else:
      v_targets = v_turns

    # Find the minimum required speed along the trajectory
    v_vtsc = float(np.min(v_targets))
    # Never exceed the driver's set cruise speed and maintain minimum 5 m/s (~18 km/h) crawling floor
    v_vtsc = max(5.0, v_vtsc)
    return min(v_cruise, v_vtsc)
  except Exception:
    return v_cruise


def get_max_accel(v_ego):
  return interp(v_ego, A_CRUISE_MAX_BP, A_CRUISE_MAX_VALS)


def limit_accel_in_turns(v_ego, angle_steers, a_target, CP):
  """
  This function returns a limited long acceleration allowed, depending on the existing lateral acceleration
  this should avoid accelerating when losing the target in turns
  """

  a_total_max = interp(v_ego, _A_TOTAL_MAX_BP, _A_TOTAL_MAX_V)
  a_y = v_ego ** 2 * angle_steers * CV.DEG_TO_RAD / (CP.steerRatio * CP.wheelbase)
  a_x_allowed = math.sqrt(max(a_total_max ** 2 - a_y ** 2, 0.))

  return [a_target[0], min(a_target[1], a_x_allowed)]


class Planner:
  def __init__(self, CP, init_v=0.0, init_a=0.0):
    self.CP = CP
    self.mpc = LongitudinalMpc()

    self.fcw = False

    self.a_desired = init_a
    self.v_desired_filter = FirstOrderFilter(init_v, 2.0, DT_MDL)

    self.v_desired_trajectory = np.zeros(CONTROL_N)
    self.a_desired_trajectory = np.zeros(CONTROL_N)
    self.j_desired_trajectory = np.zeros(CONTROL_N)

  def update(self, sm, curvatures=None, t_idxs=None, vtsc_mode=0, e2e_active=False):
    v_ego = sm['carState'].vEgo
    a_ego = sm['carState'].aEgo

    v_cruise_kph = sm['controlsState'].vCruise
    v_cruise_kph = min(v_cruise_kph, V_CRUISE_MAX)
    v_cruise = v_cruise_kph * CV.KPH_TO_MS

    # Apply Vision Turn Speed Control (VTSC)
    v_cruise = eval_vtsc(v_ego, v_cruise, curvatures, t_idxs, vtsc_mode, e2e_active)

    long_control_state = sm['controlsState'].longControlState
    force_slow_decel = sm['controlsState'].forceDecel

    prev_accel_constraint = True
    if long_control_state == LongCtrlState.off:
      self.v_desired_filter.x = v_ego
      self.a_desired = a_ego
      # Smoothly changing between accel trajectory is only relevant when OP is driving
      prev_accel_constraint = False

    # Prevent divergence, smooth in current v_ego
    self.v_desired_filter.x = max(0.0, self.v_desired_filter.update(v_ego))

    accel_limits = [A_CRUISE_MIN, get_max_accel(v_ego)]
    accel_limits_turns = limit_accel_in_turns(v_ego, sm['carState'].steeringAngleDeg, accel_limits, self.CP)
    if force_slow_decel:
      # if required so, force a smooth deceleration
      accel_limits_turns[1] = min(accel_limits_turns[1], AWARENESS_DECEL)
      accel_limits_turns[0] = min(accel_limits_turns[0], accel_limits_turns[1])
    # clip limits, cannot init MPC outside of bounds
    accel_limits_turns[0] = min(accel_limits_turns[0], self.a_desired + 0.05)
    accel_limits_turns[1] = max(accel_limits_turns[1], self.a_desired - 0.05)
    self.mpc.set_accel_limits(accel_limits_turns[0], accel_limits_turns[1])
    self.mpc.set_cur_state(self.v_desired_filter.x, self.a_desired)
    self.mpc.update(sm['carState'], sm['radarState'], v_cruise, prev_accel_constraint=prev_accel_constraint)
    self.v_desired_trajectory = np.interp(T_IDXS[:CONTROL_N], T_IDXS_MPC, self.mpc.v_solution)
    self.a_desired_trajectory = np.interp(T_IDXS[:CONTROL_N], T_IDXS_MPC, self.mpc.a_solution)
    self.j_desired_trajectory = np.interp(T_IDXS[:CONTROL_N], T_IDXS_MPC[:-1], self.mpc.j_solution)

    # TODO counter is only needed because radar is glitchy, remove once radar is gone
    self.fcw = self.mpc.crash_cnt > 5
    if self.fcw:
      cloudlog.info("FCW triggered")

    # Interpolate 0.05 seconds and save as starting point for next iteration
    a_prev = self.a_desired
    self.a_desired = float(interp(DT_MDL, T_IDXS[:CONTROL_N], self.a_desired_trajectory))
    self.v_desired_filter.x = self.v_desired_filter.x + DT_MDL * (self.a_desired + a_prev) / 2.0

  def publish(self, sm, pm):
    plan_send = messaging.new_message('longitudinalPlan')

    plan_send.valid = sm.all_alive_and_valid(service_list=['carState', 'controlsState'])

    longitudinalPlan = plan_send.longitudinalPlan
    longitudinalPlan.modelMonoTime = sm.logMonoTime['modelV2']
    longitudinalPlan.processingDelay = (plan_send.logMonoTime / 1e9) - sm.logMonoTime['modelV2']

    longitudinalPlan.speeds = self.v_desired_trajectory.tolist()
    longitudinalPlan.accels = self.a_desired_trajectory.tolist()
    longitudinalPlan.jerks = self.j_desired_trajectory.tolist()

    longitudinalPlan.hasLead = sm['radarState'].leadOne.status
    longitudinalPlan.longitudinalPlanSource = self.mpc.source
    longitudinalPlan.fcw = self.fcw

    longitudinalPlan.solverExecutionTime = self.mpc.solve_time

    pm.send('longitudinalPlan', plan_send)
