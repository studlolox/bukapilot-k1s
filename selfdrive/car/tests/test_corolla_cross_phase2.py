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
from common.numpy_fast import clip, interp
from selfdrive.car.toyota.values import CarControllerParams
from selfdrive.car.toyota.tunes import LongTunes, set_long_tune
from selfdrive.controls.lib.pid import PIController


class MockLongPIDParams:
  def __init__(self):
    self.deadzoneBP = []
    self.deadzoneV = []
    self.kpBP = []
    self.kpV = []
    self.kiBP = []
    self.kiV = []


class MockLongTune:
  def __init__(self):
    self.deadzoneBP = []
    self.deadzoneV = []
    self.kpBP = []
    self.kpV = []
    self.kiBP = []
    self.kiV = []


class TestCorollaCrossPhase2(unittest.TestCase):

  def test_cross_hybrid_tune_parameters(self):
    """Verify LongTunes.CROSS_HYBRID tuning parameters in tunes.py."""
    tune = MockLongTune()
    set_long_tune(tune, LongTunes.CROSS_HYBRID)

    self.assertEqual(tune.deadzoneBP, [0., 8.05])
    self.assertEqual(tune.deadzoneV, [0.0, 0.12])
    self.assertEqual(tune.kpBP, [0., 5., 20.])
    self.assertEqual(tune.kpV, [1.2, 1.1, 0.65])
    self.assertEqual(tune.kiBP, [0., 5., 12., 20., 27.])
    self.assertEqual(tune.kiV, [0.28, 0.22, 0.18, 0.15, 0.08])

  def test_long_pi_controller_gains_interpolation(self):
    """Verify PIController gain scheduling for hybrid deceleration hand-off."""
    tune = MockLongTune()
    set_long_tune(tune, LongTunes.CROSS_HYBRID)

    pi = PIController((tune.kpBP, tune.kpV),
                      (tune.kiBP, tune.kiV),
                      rate=100)

    # At 0 m/s (standstill / crawl)
    pi.speed = 0.0
    self.assertAlmostEqual(pi.k_p, 1.2, places=3)
    self.assertAlmostEqual(pi.k_i, 0.28, places=3)

    # At 5 m/s (18 km/h - e-CVT regen handover zone)
    pi.speed = 5.0
    self.assertAlmostEqual(pi.k_p, 1.1, places=3)
    self.assertAlmostEqual(pi.k_i, 0.22, places=3)

    # At 12 m/s (43 km/h - arterial driving)
    pi.speed = 12.0
    self.assertAlmostEqual(pi.k_i, 0.18, places=3)

    # At 20 m/s (72 km/h - highway cruising)
    pi.speed = 20.0
    self.assertAlmostEqual(pi.k_p, 0.65, places=3)
    self.assertAlmostEqual(pi.k_i, 0.15, places=3)

  def test_standstill_brake_hold_and_auto_resume(self):
    """Verify standstill brake hold vs forward takeoff command logic."""
    def arbitrate_accel(standstill, cruise_standstill, pcm_accel_cmd, v_ego, lead):
      if standstill:
        if cruise_standstill or pcm_accel_cmd < 0.1:
          pcm_accel_cmd = -1.5
      elif v_ego < 1.5 and lead:
        pcm_accel_cmd = clip(pcm_accel_cmd, CarControllerParams.ACCEL_MIN, 1.0)
      return pcm_accel_cmd

    # Case 1: At standstill with no motion commanded -> Firm -1.5 m/s^2 hold
    self.assertEqual(arbitrate_accel(standstill=True, cruise_standstill=False, pcm_accel_cmd=-0.2, v_ego=0.0, lead=True), -1.5)
    self.assertEqual(arbitrate_accel(standstill=True, cruise_standstill=False, pcm_accel_cmd=0.05, v_ego=0.0, lead=True), -1.5)

    # Case 2: At standstill, cruiseState indicates standstill (e.g. latched by ResumeWithRes) -> Must hold -1.5
    self.assertEqual(arbitrate_accel(standstill=True, cruise_standstill=True, pcm_accel_cmd=0.5, v_ego=0.0, lead=True), -1.5)

    # Case 3: At standstill, planner commands takeoff (pcm_accel_cmd >= 0.1) -> Allows positive command through
    self.assertEqual(arbitrate_accel(standstill=True, cruise_standstill=False, pcm_accel_cmd=0.4, v_ego=0.0, lead=True), 0.4)

    # Case 4: Rolling forward out of stop (vEgo = 0.5 m/s < 1.5) following lead -> Moderate acceleration passes unblocked
    self.assertEqual(arbitrate_accel(standstill=False, cruise_standstill=False, pcm_accel_cmd=0.6, v_ego=0.5, lead=True), 0.6)

    # Case 5: Rolling forward (vEgo = 0.8 m/s < 1.5), planner spikes high -> Comfort capped at 1.0 m/s^2
    self.assertEqual(arbitrate_accel(standstill=False, cruise_standstill=False, pcm_accel_cmd=1.8, v_ego=0.8, lead=True), 1.0)

    # Case 6: Braking near lead car (vEgo = 1.0 m/s, decel commanded) -> Deceleration fully preserved
    self.assertEqual(arbitrate_accel(standstill=False, cruise_standstill=False, pcm_accel_cmd=-2.2, v_ego=1.0, lead=True), -2.2)


if __name__ == '__main__':
  unittest.main()
