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
  sys.modules['cereal.messaging'] = cereal_mock.messaging
if 'opendbc.can.packer' not in sys.modules:
  sys.modules['opendbc.can.packer'] = MagicMock()
if 'casadi' not in sys.modules:
  sys.modules['casadi'] = MagicMock()
if 'selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc' not in sys.modules:
  sys.modules['selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc'] = MagicMock()
if 'selfdrive.controls.lib.longitudinal_mpc_lib.c_generated_code' not in sys.modules:
  sys.modules['selfdrive.controls.lib.longitudinal_mpc_lib.c_generated_code'] = MagicMock()
if 'selfdrive.controls.lib.longitudinal_mpc_lib.c_generated_code.acados_ocp_solver_pyx' not in sys.modules:
  sys.modules['selfdrive.controls.lib.longitudinal_mpc_lib.c_generated_code.acados_ocp_solver_pyx'] = MagicMock()

import unittest
import numpy as np
from selfdrive.controls.lib.longitudinal_planner import eval_vtsc, VTSC_A_LAT_MAX, VTSC_A_DECEL
from selfdrive.monitoring.driver_monitor import DRIVER_MONITOR_SETTINGS, DriverStatus, DistractedType


class MockPose:
  def __init__(self, pitch, yaw, roll=0.0):
    self.pitch = pitch
    self.yaw = yaw
    self.roll = roll
    self.cfactor_pitch = 1.0
    self.cfactor_yaw = 1.0


class MockBlink:
  def __init__(self):
    self.left_blink = 0.0
    self.right_blink = 0.0
    self.cfactor = 1.0


class TestCorollaCrossPhase4(unittest.TestCase):

  def test_vtsc_curvature_speed_reduction(self):
    """Verify Vision-Turn Speed Control (VTSC) calculations for SUV roll limits."""
    self.assertEqual(VTSC_A_LAT_MAX, 2.0)
    self.assertEqual(VTSC_A_DECEL, 1.3)

    v_cruise = 30.0  # 108 km/h cruise set speed

    # 1. Straight road: curvature = 0 -> cruise speed maintained
    curvatures = [0.0] * 10
    t_idxs = list(range(10))
    v_out = eval_vtsc(v_ego=30.0, v_cruise=v_cruise, curvatures=curvatures, t_idxs=t_idxs, vtsc_mode=2, e2e_active=False)
    self.assertAlmostEqual(v_out, v_cruise, places=2)

    # 2. Cloverleaf / sharp curve: R = 100m -> curvature = 0.01
    # v_turn = sqrt(2.0 / 0.01) = sqrt(200) ≈ 14.14 m/s (51 km/h)
    curvatures = [0.01] * 10
    t_idxs = [0.0] * 10
    v_out = eval_vtsc(v_ego=30.0, v_cruise=v_cruise, curvatures=curvatures, t_idxs=t_idxs, vtsc_mode=2, e2e_active=False)
    self.assertAlmostEqual(v_out, 14.14, places=1)

    # 3. Mode 2 (Always On) must work when e2e_active is False (standard highway laneline driving)
    v_out_e2e_off = eval_vtsc(v_ego=30.0, v_cruise=v_cruise, curvatures=curvatures, t_idxs=t_idxs, vtsc_mode=2, e2e_active=False)
    self.assertAlmostEqual(v_out_e2e_off, 14.14, places=1)

    # 4. Mode 0 (Off) ignores curvature
    v_out_off = eval_vtsc(v_ego=30.0, v_cruise=v_cruise, curvatures=curvatures, t_idxs=t_idxs, vtsc_mode=0, e2e_active=False)
    self.assertAlmostEqual(v_out_off, v_cruise, places=2)

    # 5. Minimum crawl speed floor: extreme hairpin (R = 10m, curvature = 0.10) -> v_turn = sqrt(20) ≈ 4.47 m/s -> clamped to 5.0 m/s minimum
    curvatures_hairpin = [0.10] * 10
    v_out_hairpin = eval_vtsc(v_ego=20.0, v_cruise=v_cruise, curvatures=curvatures_hairpin, t_idxs=t_idxs, vtsc_mode=2, e2e_active=False)
    self.assertAlmostEqual(v_out_hairpin, 5.0, places=2)

  def test_driver_monitoring_pitch_calibration(self):
    """Verify Driver Monitoring head pitch thresholds and crossover gaze angle tolerance."""
    from unittest.mock import patch
    with patch('selfdrive.monitoring.driver_monitor.Features') as mock_features, \
         patch('selfdrive.monitoring.driver_monitor.Params') as mock_params:
      mock_features.return_value.has.return_value = False
      mock_params.return_value.get_bool.return_value = False
      settings = DRIVER_MONITOR_SETTINGS()
      dm = DriverStatus(settings=settings)

    # Threshold constants verification
    self.assertAlmostEqual(dm.settings._POSE_PITCH_THRESHOLD, 0.4109, places=4)
    self.assertAlmostEqual(dm.settings._POSE_PITCH_THRESHOLD_SLACK, 0.4529, places=4)
    self.assertAlmostEqual(dm.settings._PITCH_MIN_OFFSET, -0.175, places=3)

    blink = MockBlink()

    # Case A: Driver in crossover seating looking forward/cluster (-0.30 rad / -17.2°)
    # Under old threshold (0.3237), this caused false positive alerts
    # Under calibrated threshold (0.4109), this is properly recognized as NOT_DISTRACTED
    # Note: pitch_error = abs(-0.30 - 0.057) = 0.357 rad, which is < 0.4109 (PASS) but was > 0.3237 (FAIL previously)
    pose_normal_crossover = MockPose(pitch=-0.30, yaw=0.0)
    status_crossover = dm._is_driver_distracted(pose_normal_crossover, blink)
    self.assertEqual(status_crossover, DistractedType.NOT_DISTRACTED)

    # Case B: Driver looking severely downward (-0.50 rad / -28.6°, phone in lap)
    # Exceeds 0.4109 threshold -> correctly flagged as BAD_POSE
    pose_phone_lap = MockPose(pitch=-0.50, yaw=0.0)
    status_phone = dm._is_driver_distracted(pose_phone_lap, blink)
    self.assertEqual(status_phone, DistractedType.BAD_POSE)

    # Case C: Driver looking sideways (yaw = 0.50 rad, error = abs(0.50 - 0.11) = 0.39 rad > 0.3109) -> correctly flagged as BAD_POSE
    pose_sideways = MockPose(pitch=0.0, yaw=0.50)
    status_yaw = dm._is_driver_distracted(pose_sideways, blink)
    self.assertEqual(status_yaw, DistractedType.BAD_POSE)


if __name__ == '__main__':
  unittest.main()
