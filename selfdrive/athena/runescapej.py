import hashlib
from common.params import Params


def register_user(imei, serial):
  params = Params()
  raw_id = ((imei or "") + serial) if imei else serial
  dongle_id = hashlib.sha224(raw_id.encode()).hexdigest()[:16]
  params.put("DongleId", dongle_id)
  return dongle_id


