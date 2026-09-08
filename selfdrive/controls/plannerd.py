#!/usr/bin/env python3
from cereal import car
from common.params import Params
from common.realtime import Priority, config_realtime_process
from selfdrive.swaglog import cloudlog
from selfdrive.controls.lib.longitudinal_planner import Planner
from selfdrive.controls.lib.lateral_planner import LateralPlanner
from selfdrive.hardware import TICI
import cereal.messaging as messaging


def plannerd_thread(sm=None, pm=None):
  config_realtime_process(5 if TICI else 2, Priority.CTRL_LOW)

  cloudlog.info("plannerd is waiting for CarParams")
  params = Params()
  CP = car.CarParams.from_bytes(params.get("CarParams", block=True))
  cloudlog.info("plannerd got CarParams: %s", CP.carName)

  use_lanelines = not params.get_bool('EndToEndToggle')
  wide_camera = params.get_bool('EnableWideCamera') if TICI else False
  try:
    vtsc_mode = int(params.get("VisionTurnSpeedControl") or "2")
  except Exception:
    vtsc_mode = 2

  cloudlog.event("e2e mode", on=(not use_lanelines))

  longitudinal_planner = Planner(CP)
  lateral_planner = LateralPlanner(CP, use_lanelines=use_lanelines, wide_camera=wide_camera)

  if sm is None:
    sm = messaging.SubMaster(['carState', 'controlsState', 'radarState', 'modelV2'],
                             poll=['radarState', 'modelV2'], ignore_avg_freq=['radarState'])

  if pm is None:
    pm = messaging.PubMaster(['longitudinalPlan', 'lateralPlan'])

  while True:
    sm.update()

    if sm.updated['modelV2']:
      if sm.frame % 20 == 0:
        lateral_planner.use_lanelines = not params.get_bool('EndToEndToggle')
        try:
          vtsc_mode = int(params.get("VisionTurnSpeedControl") or "2")
        except Exception:
          vtsc_mode = 2

      lateral_planner.update(sm)
      lateral_planner.publish(sm, pm)
      longitudinal_planner.update(sm,
                                  curvatures=lateral_planner.curvatures,
                                  t_idxs=lateral_planner.t_idxs,
                                  vtsc_mode=vtsc_mode,
                                  e2e_active=(not lateral_planner.use_lanelines))
      longitudinal_planner.publish(sm, pm)


def main(sm=None, pm=None):
  plannerd_thread(sm, pm)


if __name__ == "__main__":
  main()
