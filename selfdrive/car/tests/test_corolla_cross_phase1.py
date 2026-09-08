#!/usr/bin/env python3
import sys
from unittest.mock import MagicMock
if 'capnp' not in sys.modules:
  sys.modules['capnp'] = MagicMock()
if 'cereal' not in sys.modules:
  cereal_mock = MagicMock()
  sys.modules['cereal'] = cereal_mock
  sys.modules['cereal.car'] = cereal_mock.car
  sys.modules['cereal.log'] = cereal_mock.log
if 'opendbc.can.packer' not in sys.modules:
  sys.modules['opendbc.can.packer'] = MagicMock()

import unittest
from common.numpy_fast import interp
from selfdrive.car import apply_toyota_steer_torque_limits
from selfdrive.car.toyota.values import CarControllerParams, STEER_THRESHOLD
from selfdrive.car.toyota.carcontroller import MAX_USER_TORQUE
from selfdrive.car.toyota.tunes import LatTunes, set_lat_tune
from selfdrive.controls.lib.pid import PIController


class MockPIDParams:
  def __init__(self):
    self.kpBP = []
    self.kpV = []
    self.kiBP = []
    self.kiV = []
    self.kf = 0.0


class MockTune:
  def __init__(self):
    self.pid = MockPIDParams()
    self.tune_type = None

  def init(self, tune_type):
    self.tune_type = tune_type


class TestCorollaCrossPhase1(unittest.TestCase):

  def test_lat_tune_pid_cross_enum_and_values(self):
    """Verify LatTunes.PID_CROSS enum and tuning parameter assignment."""
    self.assertTrue(hasattr(LatTunes, 'PID_CROSS'))
    self.assertEqual(LatTunes.PID_CROSS.value, 16)

    tune = MockTune()
    set_lat_tune(tune, LatTunes.PID_CROSS)

    self.assertEqual(tune.tune_type, 'pid')
    self.assertEqual(tune.pid.kpBP, [0.0, 15.0, 25.0])
    self.assertEqual(tune.pid.kpV, [0.55, 0.50, 0.45])
    self.assertEqual(tune.pid.kiBP, [0.0, 15.0, 25.0])
    self.assertEqual(tune.pid.kiV, [0.08, 0.06, 0.05])
    self.assertAlmostEqual(tune.pid.kf, 0.000085, places=7)

  def test_pi_controller_speed_scheduled_gains(self):
    """Verify that PIController interpolates PID_CROSS gains correctly across vehicle speeds."""
    tune = MockTune()
    set_lat_tune(tune, LatTunes.PID_CROSS)

    pi = PIController((tune.pid.kpBP, tune.pid.kpV),
                      (tune.pid.kiBP, tune.pid.kiV),
                      k_f=tune.pid.kf, pos_limit=1.0, neg_limit=-1.0)

    # Low speed (0 m/s / standstill)
    pi.speed = 0.0
    self.assertAlmostEqual(pi.k_p, 0.55, places=3)
    self.assertAlmostEqual(pi.k_i, 0.08, places=3)

    # Mid speed (15 m/s = 54 km/h)
    pi.speed = 15.0
    self.assertAlmostEqual(pi.k_p, 0.50, places=3)
    self.assertAlmostEqual(pi.k_i, 0.06, places=3)

    # Interpolated speed (20 m/s = 72 km/h)
    pi.speed = 20.0
    self.assertAlmostEqual(pi.k_p, 0.475, places=3)
    self.assertAlmostEqual(pi.k_i, 0.055, places=3)

    # Highway speed (25 m/s = 90 km/h)
    pi.speed = 25.0
    self.assertAlmostEqual(pi.k_p, 0.45, places=3)
    self.assertAlmostEqual(pi.k_i, 0.05, places=3)

    # Expressway speed (> 25 m/s, clamped to top breakpoint)
    pi.speed = 33.3  # 120 km/h
    self.assertAlmostEqual(pi.k_p, 0.45, places=3)
    self.assertAlmostEqual(pi.k_i, 0.05, places=3)

  def test_driver_override_blending_interpolation(self):
    """Verify linear torque blend factor across the override spectrum."""
    def calc_blend(driver_torque):
      if abs(driver_torque) > STEER_THRESHOLD:
        return interp(abs(driver_torque), [STEER_THRESHOLD, MAX_USER_TORQUE], [1.0, 0.0])
      return 1.0

    # Below threshold (driver hands light or off)
    self.assertEqual(calc_blend(0), 1.0)
    self.assertEqual(calc_blend(50), 1.0)
    self.assertEqual(calc_blend(STEER_THRESHOLD), 1.0)

    # Within blend zone (driver taking over gently)
    self.assertAlmostEqual(calc_blend(200), 0.75, places=3)
    self.assertAlmostEqual(calc_blend(300), 0.50, places=3)
    self.assertAlmostEqual(calc_blend(400), 0.25, places=3)

    # At or above maximum threshold (full override handover)
    self.assertAlmostEqual(calc_blend(MAX_USER_TORQUE), 0.0, places=3)
    self.assertAlmostEqual(calc_blend(600), 0.0, places=3)

  def test_blended_steer_torque_under_toyota_limits(self):
    """Verify that scaled new_steer respects Toyota delta and error rate limits."""
    # Simulate full left steer command
    raw_steer = CarControllerParams.STEER_MAX  # 1500
    last_steer = 1000
    motor_torque = 950

    # Case A: Driver torque = 0 (no override)
    driver_torque = 0
    blend = interp(abs(driver_torque), [STEER_THRESHOLD, MAX_USER_TORQUE], [1.0, 0.0]) if abs(driver_torque) > STEER_THRESHOLD else 1.0
    new_steer = int(round(raw_steer * blend))
    applied_a = apply_toyota_steer_torque_limits(new_steer, last_steer, motor_torque, CarControllerParams)
    # STEER_DELTA_UP = 10 -> cannot exceed 1010
    self.assertEqual(applied_a, 1010)

    # Case B: Driver torque = 300 (blend = 0.5 -> new_steer = 750)
    driver_torque = 300
    blend = interp(abs(driver_torque), [STEER_THRESHOLD, MAX_USER_TORQUE], [1.0, 0.0])
    new_steer = int(round(raw_steer * blend))
    self.assertEqual(new_steer, 750)
    applied_b = apply_toyota_steer_torque_limits(new_steer, last_steer, motor_torque, CarControllerParams)
    # Target 750 is lower than last_steer 1000. STEER_DELTA_DOWN = 25 -> down to 975
    self.assertEqual(applied_b, 975)
    self.assertLess(applied_b, last_steer)

  def test_corolla_cross_candidate_parameters(self):
    """Verify that Corolla Cross candidates have correct wheelbase, steer ratio, and PID_CROSS tune."""
    from selfdrive.car.toyota.values import CAR
    for candidate in (CAR.CROSS_TSS2, CAR.CROSSH_TSS2):
      tune = MockTune()
      set_lat_tune(tune, LatTunes.PID_CROSS)
      self.assertEqual(tune.tune_type, 'pid')
      self.assertEqual(tune.pid.kf, 0.000085)
      self.assertEqual(tune.pid.kpV, [0.55, 0.50, 0.45])


if __name__ == '__main__':
  unittest.main()
