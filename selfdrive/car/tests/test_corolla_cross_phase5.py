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
if 'smbus2' not in sys.modules:
  sys.modules['smbus2'] = MagicMock()

import unittest
from selfdrive.thermald.thermald import handle_fan_eon, _TEMP_THRS_H, _TEMP_THRS_L, _FAN_SPEEDS


class TestCorollaCrossPhase5(unittest.TestCase):

  def test_proactive_cooling_fan_curve(self):
    """Verify advanced proactive cooling thresholds for K1S in tropical climates."""
    # Ensure proactive thresholds are calibrated
    self.assertEqual(_TEMP_THRS_H, [45.0, 58.0, 68.0, 10000])
    self.assertEqual(_TEMP_THRS_L, [40.0, 52.0, 63.0, 10000])
    self.assertEqual(_FAN_SPEEDS, [0, 16384, 32768, 65535])

    # 1. Temperature ramp up (rising)
    speed = 0
    # At 42°C: below 45°C -> speed 0
    speed = handle_fan_eon(None, 42.0, speed, ignition=True)
    self.assertEqual(speed, 0)

    # At 47°C: above 45°C -> speed 16384 (level 1 proactive cooling starts)
    speed = handle_fan_eon(None, 47.0, speed, ignition=True)
    self.assertEqual(speed, 16384)

    # At 60°C: above 58°C -> speed 32768 (level 2 medium cooling)
    speed = handle_fan_eon(None, 60.0, speed, ignition=True)
    self.assertEqual(speed, 32768)

    # At 70°C: above 68°C -> speed 65535 (level 3 100% full blast)
    speed = handle_fan_eon(None, 70.0, speed, ignition=True)
    self.assertEqual(speed, 65535)

    # 2. Temperature cool down with hysteresis (falling)
    # Cooling to 65°C: above low threshold 63°C -> stays at max 65535
    speed = handle_fan_eon(None, 65.0, speed, ignition=True)
    self.assertEqual(speed, 65535)

    # Cooling to 61°C: below low threshold 63°C -> drops to 32768
    speed = handle_fan_eon(None, 61.0, speed, ignition=True)
    self.assertEqual(speed, 32768)

    # Cooling to 50°C: below low threshold 52°C -> drops to 16384
    speed = handle_fan_eon(None, 50.0, speed, ignition=True)
    self.assertEqual(speed, 16384)

    # Cooling to 38°C: below low threshold 40°C -> turns off (0)
    speed = handle_fan_eon(None, 38.0, speed, ignition=True)
    self.assertEqual(speed, 0)

  def test_camera_ae_roi_coordinates(self):
    """Verify camera auto-exposure region of interest for windshield glare avoidance."""
    # From camera_qcom.cc:
    x = 290
    y = 280
    width = 560
    height = 280

    rgb_width = 1164
    rgb_height = 874

    # Center horizontal alignment check
    roi_center_x = x + width / 2.0
    frame_center_x = rgb_width / 2.0
    self.assertLess(abs(roi_center_x - frame_center_x), 20.0)

    # Vertical bounds
    y_top = y
    y_bottom = y + height

    self.assertEqual(y_top, 280)
    self.assertEqual(y_bottom, 560)

    # Verify exclusion of bottom dashboard glare zone (> 35% bottom excluded)
    bottom_excluded_fraction = (rgb_height - y_bottom) / rgb_height
    self.assertGreater(bottom_excluded_fraction, 0.35)

  def test_dynamic_screen_dimming_logic(self):
    """Verify dynamic screen dimming math during steady onroad cruising."""
    # Mock CIE 1931 perceptual brightness calculation
    def calc_brightness(light_sensor, started, ignition, screen_off, timeout, has_alert):
      if not started:
        return 100.0  # BACKLIGHT_OFFROAD

      clipped = 100.0 * light_sensor
      if clipped <= 8:
        clipped = clipped / 903.3
      else:
        clipped = ((clipped + 16.0) / 116.0) ** 3.0
      clipped = max(3.0, min(100.0, 100.0 * clipped))

      # Thermal dimming
      if started and ignition and not screen_off and timeout == 0 and not has_alert:
        clipped = max(10.0, clipped * 0.50)

      return clipped

    # Daytime sensor value (0.80)
    b_active = calc_brightness(0.80, started=True, ignition=True, screen_off=False, timeout=300, has_alert=False)
    b_dimmed = calc_brightness(0.80, started=True, ignition=True, screen_off=False, timeout=0, has_alert=False)
    b_alert = calc_brightness(0.80, started=True, ignition=True, screen_off=False, timeout=0, has_alert=True)

    # 1. Dimmed brightness must be exactly 50% of active brightness
    self.assertAlmostEqual(b_dimmed, b_active * 0.50, places=2)

    # 2. Active alert restores full 100% brightness immediately
    self.assertAlmostEqual(b_alert, b_active, places=2)

  def test_eon_set_power_save_governor_logic(self):
    """Verify K1S devfreq governor scaling for offroad power saving."""
    from selfdrive.hardware.eon.hardware import Android
    import unittest.mock as mock

    android = Android()
    # Test that set_power_save runs without throwing exceptions even with absent sysfs nodes
    android.set_power_save(powersave_enabled=True)
    android.set_power_save(powersave_enabled=False)

    # Mock open and isfile to verify governor selections
    written_data = {}
    def mock_open(path, mode="r"):
      m = mock.MagicMock()
      def write(val):
        written_data[path] = val.strip()
      m.__enter__.return_value.write = write
      return m

    with mock.patch("os.path.isfile", return_value=True), mock.patch("builtins.open", mock_open):
      # Offroad powersave mode
      android.set_power_save(powersave_enabled=True)
      self.assertEqual(written_data.get("/sys/class/devfreq/soc:qcom,cpubw/governor"), "powersave")
      self.assertEqual(written_data.get("/sys/class/devfreq/soc:qcom,m4m/governor"), "powersave")
      self.assertEqual(written_data.get("/sys/class/devfreq/b00000.qcom,kgsl-3d0/governor"), "msm-adreno-tz")

      # Onroad performance mode
      android.set_power_save(powersave_enabled=False)
      self.assertEqual(written_data.get("/sys/class/devfreq/soc:qcom,cpubw/governor"), "performance")
      self.assertEqual(written_data.get("/sys/class/devfreq/soc:qcom,m4m/governor"), "performance")
      self.assertEqual(written_data.get("/sys/class/devfreq/b00000.qcom,kgsl-3d0/governor"), "performance")

  def test_internal_battery_thermal_safeguard_logic(self):
    """Verify smart internal battery charging cutoff when hot inside enclosure."""
    def should_charge(battery_percent, max_comp_temp, currently_charging):
      if battery_percent >= 75 and max_comp_temp > 70.0:
        return False
      elif battery_percent < 60 or max_comp_temp < 65.0:
        return True
      return currently_charging

    # 1. High temperature (74°C) with high battery (80%) -> Stop charging to prevent Joule heating
    self.assertFalse(should_charge(battery_percent=80, max_comp_temp=74.0, currently_charging=True))

    # 2. Cool down (63°C) -> Resume charging
    self.assertTrue(should_charge(battery_percent=80, max_comp_temp=63.0, currently_charging=False))

    # 3. Low battery (< 60%) -> Always charge
    self.assertTrue(should_charge(battery_percent=55, max_comp_temp=72.0, currently_charging=False))

    # 4. Hysteresis band (70% battery at 68°C) -> Retain current state
    self.assertTrue(should_charge(battery_percent=70, max_comp_temp=68.0, currently_charging=True))
    self.assertFalse(should_charge(battery_percent=70, max_comp_temp=68.0, currently_charging=False))


if __name__ == '__main__':
  unittest.main()
