#!/usr/bin/env python3
import hashlib
import time
from pathlib import Path

from common.params import Params
from common.spinner import Spinner
from common.basedir import PERSIST
from selfdrive.controls.lib.alertmanager import set_offroad_alert
from selfdrive.hardware import HARDWARE, PC
from selfdrive.swaglog import cloudlog


UNREGISTERED_DONGLE_ID = "UnregisteredDevice"


def is_registered_device() -> bool:
  dongle = Params().get("DongleId", encoding='utf-8')
  return dongle not in (None, UNREGISTERED_DONGLE_ID)


def get_deterministic_dongle_id(imei: str, serial: str) -> str:
  raw_id = (imei + serial) if imei else serial
  return hashlib.sha224(raw_id.encode()).hexdigest()[:16]


def register(show_spinner=False) -> str:
  params = Params()
  params.put("SubscriberInfo", HARDWARE.get_subscriber_info())

  IMEI = params.get("IMEI", encoding='utf8')
  HardwareSerial = params.get("HardwareSerial", encoding='utf8')
  dongle_id = params.get("DongleId", encoding='utf8')
  needs_registration = None in (IMEI, HardwareSerial, dongle_id) or dongle_id == UNREGISTERED_DONGLE_ID

  pubkey = Path(PERSIST + "/comma/id_rsa.pub")
  if not pubkey.is_file():
    cloudlog.warning(f"missing public key: {pubkey}")

  if needs_registration:
    if show_spinner:
      spinner = Spinner()
      spinner.update("registering device")

    serial = HARDWARE.get_serial()
    imei = IMEI

    # On real hardware, attempt to query IMEI if not already stored
    if not imei and not PC:
      start_time = time.monotonic()
      while time.monotonic() - start_time < 5:
        try:
          imei1, imei2 = HARDWARE.get_imei(0), HARDWARE.get_imei(1)
          imei = imei2 or imei1
          if imei:
            break
        except Exception:
          cloudlog.exception("Error getting imei, retrying...")
        time.sleep(0.5)

    if imei:
      params.put("IMEI", imei)
    params.put("HardwareSerial", serial)

    # Deterministic local Dongle ID derivation
    dongle_id = get_deterministic_dongle_id(imei, serial)

    if show_spinner:
      spinner.close()

  if dongle_id:
    params.put("DongleId", dongle_id)
    # Always silence the unofficial hardware alert in ezpilot standalone mode
    set_offroad_alert("Offroad_UnofficialHardware", False)

  return dongle_id


if __name__ == "__main__":
  print(register())

