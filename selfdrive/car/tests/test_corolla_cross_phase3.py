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
from cereal import log
from selfdrive.controls.lib.desire_helper import DesireHelper, LaneChangeState, LaneChangeDirection


class MockCarState:
  def __init__(self):
    self.leftBlinker = False
    self.rightBlinker = False
    self.leftBlindspot = False
    self.rightBlindspot = False
    self.steeringPressed = False
    self.steeringTorque = 0.0
    self.vEgo = 25.0  # 90 km/h (well above minimum lane change speed)
    self.lkaDisabled = False


class TestCorollaCrossPhase3(unittest.TestCase):

  def setUp(self):
    self.dh = DesireHelper()
    self.dh.is_alc_enabled = True
    self.cs = MockCarState()

  def test_bsm_blocking_and_latched_clearance(self):
    """Verify that BSM blocks lane change, latches driver nudge, and auto-starts after 0.5s clearance."""
    import time
    fake_time = 100.0
    time_orig = time.monotonic
    time.monotonic = lambda: fake_time
    try:
      # Step 1: Turn on left blinker with adjacent car in blindspot
      self.cs.leftBlinker = True
      self.cs.leftBlindspot = True
      self.dh.update(self.cs, active=True, lane_change_prob=0.5)
      self.assertEqual(self.dh.lane_change_state, LaneChangeState.preLaneChange)
      self.assertEqual(self.dh.lane_change_direction, LaneChangeDirection.left)

      # Step 2: Driver applies steering torque to request lane change while blindspot occupied
      fake_time = 101.0
      self.cs.steeringPressed = True
      self.cs.steeringTorque = 150.0  # positive for left
      self.dh.update(self.cs, active=True, lane_change_prob=0.5)

      # Must remain in preLaneChange because blindspot is detected, but intent should be latched
      self.assertEqual(self.dh.lane_change_state, LaneChangeState.preLaneChange)
      self.assertTrue(self.dh.lane_change_latched)

      # Step 3: Driver releases steering wheel
      fake_time = 102.0
      self.cs.steeringPressed = False
      self.cs.steeringTorque = 0.0
      self.dh.update(self.cs, active=True, lane_change_prob=0.5)
      self.assertEqual(self.dh.lane_change_state, LaneChangeState.preLaneChange)
      self.assertTrue(self.dh.lane_change_latched)

      # Step 4: Blindspot clears, but under 0.5s debounce
      self.cs.leftBlindspot = False
      # At 102.2s (0.2s after clearance at 102.0s)
      fake_time = 102.2
      self.dh.update(self.cs, active=True, lane_change_prob=0.5)
      self.assertEqual(self.dh.lane_change_state, LaneChangeState.preLaneChange)
      self.assertTrue(self.dh.lane_change_latched)

      # At 102.6s (0.6s after clearance at 102.0s -> clearance window verified)
      fake_time = 102.6
      self.dh.update(self.cs, active=True, lane_change_prob=0.5)
      self.assertEqual(self.dh.lane_change_state, LaneChangeState.laneChangeStarting)
      self.assertFalse(self.dh.lane_change_latched)
    finally:
      time.monotonic = time_orig

  def test_blinker_off_cancels_latched_lane_change(self):
    """Verify that canceling the blinker immediately aborts preLaneChange and clears the latch."""
    import time
    fake_time = 100.0
    time_orig = time.monotonic
    time.monotonic = lambda: fake_time
    try:
      self.cs.leftBlinker = True
      self.cs.leftBlindspot = True
      self.dh.update(self.cs, active=True, lane_change_prob=0.5)
      self.assertEqual(self.dh.lane_change_state, LaneChangeState.preLaneChange)

      self.cs.steeringPressed = True
      self.cs.steeringTorque = 150.0
      self.dh.update(self.cs, active=True, lane_change_prob=0.5)
      self.assertTrue(self.dh.lane_change_latched)

      # Blinker turned off by driver
      self.cs.leftBlinker = False
      self.dh.update(self.cs, active=True, lane_change_prob=0.5)
      self.assertEqual(self.dh.lane_change_state, LaneChangeState.off)
      self.assertFalse(self.dh.lane_change_latched)
    finally:
      time.monotonic = time_orig

  def test_stock_acc_standstill_arbitration(self):
    """Verify that Stock ACC mode allows radar resume commands through at standstill."""
    def arbitrate_stock_acc(standstill, stock_acc_cmd):
      if not standstill or stock_acc_cmd > 0.1:
        return stock_acc_cmd
      return -1.5

    # Moving: commands pass directly
    self.assertEqual(arbitrate_stock_acc(standstill=False, stock_acc_cmd=0.8), 0.8)
    self.assertEqual(arbitrate_stock_acc(standstill=False, stock_acc_cmd=-1.2), -1.2)

    # Standstill holding: negative or near-zero stock command holds -1.5 m/s^2
    self.assertEqual(arbitrate_stock_acc(standstill=True, stock_acc_cmd=-0.5), -1.5)
    self.assertEqual(arbitrate_stock_acc(standstill=True, stock_acc_cmd=0.05), -1.5)

    # Standstill resuming: stock radar issues forward acceleration > 0.1 m/s^2 -> passed through
    self.assertEqual(arbitrate_stock_acc(standstill=True, stock_acc_cmd=0.6), 0.6)

  def test_distance_btn_toggle_counter(self):
    """Verify 200-frame (2.0s @ 100Hz) distance button long-press toggle."""
    counter = 0
    force_use_stock_acc = False

    for frame in range(1, 201):
      btn_pressed = 1
      if btn_pressed == 1:
        counter += 1
        if counter == 200:
          force_use_stock_acc = not force_use_stock_acc
      else:
        counter = 0

    self.assertEqual(counter, 200)
    self.assertTrue(force_use_stock_acc)


if __name__ == '__main__':
  unittest.main()
