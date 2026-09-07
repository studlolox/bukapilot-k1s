#include "selfdrive/ui/qt/onroad.h"

#include <cmath>

#include <QDebug>

#include "selfdrive/common/timing.h"
#include "selfdrive/ui/qt/util.h"
#ifdef ENABLE_MAPS
#include "selfdrive/ui/qt/maps/map.h"
#include "selfdrive/ui/qt/maps/map_helpers.h"
#endif

OnroadWindow::OnroadWindow(QWidget *parent) : QWidget(parent) {
  QVBoxLayout *main_layout  = new QVBoxLayout(this);
  main_layout->setMargin(bdr_s);
  QStackedLayout *stacked_layout = new QStackedLayout;
  stacked_layout->setStackingMode(QStackedLayout::StackAll);
  main_layout->addLayout(stacked_layout);

  QStackedLayout *road_view_layout = new QStackedLayout;
  road_view_layout->setStackingMode(QStackedLayout::StackAll);
  nvg = new NvgWindow(VISION_STREAM_RGB_BACK, this);
  road_view_layout->addWidget(nvg);
  hud = new OnroadHud(this);
  road_view_layout->addWidget(hud);

  QWidget * split_wrapper = new QWidget;
  split = new QHBoxLayout(split_wrapper);
  split->setContentsMargins(0, 0, 0, 0);
  split->setSpacing(0);
  split->addLayout(road_view_layout);

  stacked_layout->addWidget(split_wrapper);

  alerts = new OnroadAlerts(this);
  alerts->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  stacked_layout->addWidget(alerts);

  // setup stacking order
  alerts->raise();

  setAttribute(Qt::WA_OpaquePaintEvent);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &OnroadWindow::updateState);
  QObject::connect(uiState(), &UIState::offroadTransition, this, &OnroadWindow::offroadTransition);
  QObject::connect(hud, &OnroadHud::openSettings, this, &OnroadWindow::openSettings);
}

void OnroadWindow::updateState(const UIState &s) {
  QColor bgColor = bg_colors[s.status];
  Alert alert = Alert::get(*(s.sm), s.scene.started_frame);
  if (s.sm->updated("controlsState") || !alert.equal({})) {
    if (alert.type == "controlsUnresponsive") {
      bgColor = bg_colors[STATUS_ALERT];
    } else if (alert.type == "controlsUnresponsivePermanent") {
      bgColor = bg_colors[STATUS_DISENGAGED];
    }
    alerts->updateAlert(alert, bgColor);
  }

  hud->updateState(s);

  if (bg != bgColor) {
    // repaint border
    bg = bgColor;
    update();
  }
}

void OnroadWindow::mousePressEvent(QMouseEvent* e) {
  if (map != nullptr) {
    bool sidebarVisible = geometry().x() > 0;
    map->setVisible(!sidebarVisible && !map->isVisible());
  }

  // NOTE: not propagating event to parent(HomeWindow)
  hud->manualMouseEvent(e);
}

void OnroadWindow::offroadTransition(bool offroad) {
#ifdef ENABLE_MAPS
  if (!offroad) {
    if (map == nullptr && (uiState()->prime_type || !MAPBOX_TOKEN.isEmpty())) {
      MapWindow * m = new MapWindow(get_mapbox_settings());
      map = m;

      QObject::connect(uiState(), &UIState::offroadTransition, m, &MapWindow::offroadTransition);

      m->setFixedWidth(topWidget(this)->width() / 2);
      split->addWidget(m, 0, Qt::AlignRight);

      // Make map visible after adding to split
      m->offroadTransition(offroad);
    }
  }
#endif

  alerts->updateAlert({}, bg);

  // update stream type
  bool wide_cam = Hardware::TICI() && Params().getBool("EnableWideCamera");
  nvg->setStreamType(wide_cam ? VISION_STREAM_RGB_WIDE : VISION_STREAM_RGB_BACK);
}

void OnroadWindow::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.fillRect(rect(), QColor(bg.red(), bg.green(), bg.blue(), 255));
}

// ***** onroad widgets *****

// OnroadAlerts
void OnroadAlerts::updateAlert(const Alert &a, const QColor &color) {
  if (!alert.equal(a) || color != bg) {
    alert = a;
    bg = color;
    update();
  }
}

void OnroadAlerts::paintEvent(QPaintEvent *event) {
  if (alert.size == cereal::ControlsState::AlertSize::NONE) {
    return;
  }
  static std::map<cereal::ControlsState::AlertSize, const int> alert_sizes = {
    {cereal::ControlsState::AlertSize::SMALL, 271},
    {cereal::ControlsState::AlertSize::MID, 420},
    {cereal::ControlsState::AlertSize::FULL, height()},
  };
  int h = alert_sizes[alert.size];
  QRect r = QRect(0, height() - h, width(), h);

  QPainter p(this);

  // draw background + gradient
  p.setPen(Qt::NoPen);
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);

  p.setBrush(QBrush(bg));
  p.drawRect(r);

  QLinearGradient g(0, r.y(), 0, r.bottom());
  g.setColorAt(0, QColor::fromRgbF(0, 0, 0, 0.05));
  g.setColorAt(1, QColor::fromRgbF(0, 0, 0, 0.35));

  p.setCompositionMode(QPainter::CompositionMode_DestinationOver);
  p.setBrush(QBrush(g));
  p.fillRect(r, g);
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);

  // text
  const QPoint c = r.center();
  p.setPen(QColor(0xff, 0xff, 0xff));
  p.setRenderHint(QPainter::TextAntialiasing);
  if (alert.size == cereal::ControlsState::AlertSize::SMALL) {
    configFont(p, "Open Sans", 74, "SemiBold");
    p.drawText(r, Qt::AlignCenter, alert.text1);
  } else if (alert.size == cereal::ControlsState::AlertSize::MID) {
    configFont(p, "Open Sans", 88, "Bold");
    p.drawText(QRect(0, c.y() - 125, width(), 150), Qt::AlignHCenter | Qt::AlignTop, alert.text1);
    configFont(p, "Open Sans", 66, "Regular");
    p.drawText(QRect(0, c.y() + 21, width(), 90), Qt::AlignHCenter, alert.text2);
  } else if (alert.size == cereal::ControlsState::AlertSize::FULL) {
    bool l = alert.text1.length() > 15;
    configFont(p, "Open Sans", l ? 132 : 177, "Bold");
    p.drawText(QRect(0, r.y() + (l ? 240 : 270), width(), 600), Qt::AlignHCenter | Qt::TextWordWrap, alert.text1);
    configFont(p, "Open Sans", 88, "Regular");
    p.drawText(QRect(0, r.height() - (l ? 361 : 420), width(), 300), Qt::AlignHCenter | Qt::TextWordWrap, alert.text2);
  }
}

// OnroadHud
OnroadHud::OnroadHud(QWidget *parent) : QWidget(parent) {
  dm_img = loadPixmap("../assets/img_driver_face.png", {img_size, img_size});
  settings_img = loadPixmap("../assets/kommu/settings.png", {img_size, img_size});
  wheel_img = loadPixmap("../assets/icons_mici/wheel.png", {80, 80});
  wheel_critical_img = loadPixmap("../assets/icons_mici/wheel_critical.png", {80, 80});
  turn_intent_img = loadPixmap("../assets/icons_mici/turn_intent_left.png", {38, 38});
  exclamation_img = loadPixmap("../assets/icons_mici/exclamation_point.png", {12, 44});

  connect(this, &OnroadHud::valueChanged, [=] { update(); });
}

void OnroadHud::updateState(const UIState &s) {
  const int SET_SPEED_NA = 255;
  const SubMaster &sm = *(s.sm);
  const auto cs = sm["controlsState"].getControlsState();

  float maxspeed = sm["carState"].getCarState().getCruiseState().getSpeedCluster() * MS_TO_KPH;
  bool cruise_set = maxspeed > 0 && (int)maxspeed != SET_SPEED_NA;
  if (cruise_set && !s.scene.is_metric) {
    maxspeed *= KM_TO_MILE;
  }
  QString maxspeed_str = cruise_set ? QString::number(std::nearbyint(maxspeed)) : "–";
  float cur_speed_hud = sm["carState"].getCarState().getVEgoCluster();
  float cur_speed = (cur_speed_hud == 0.0) ? sm["carState"].getCarState().getVEgo() : cur_speed_hud;
  cur_speed = std::max(0.0, cur_speed * (s.scene.is_metric ? MS_TO_KPH : MS_TO_MPH));
  float temp = sm["deviceState"].getDeviceState().getAmbientTempC() * (s.scene.is_metric ? 1 : 1.8);
  temp += s.scene.is_metric ? 0 : 32;

  float steer_angle = sm["carState"].getCarState().getSteeringAngleDeg();
  bool steer_override = sm["carState"].getCarState().getSteeringPressed();
  bool lat_active = cs.getEnabled();
  int thermal_st = (int)sm["deviceState"].getDeviceState().getThermalStatus();

  // Extract torque output and saturation from lateralControlState
  float cur_torque = 0.0f;
  bool saturated = false;
  const auto lcs = cs.getLateralControlState();
  switch (lcs.which()) {
    case cereal::ControlsState::LateralControlState::PID_STATE:
      cur_torque = lcs.getPidState().getOutput();
      saturated = lcs.getPidState().getSaturated();
      break;
    case cereal::ControlsState::LateralControlState::INDI_STATE:
      cur_torque = lcs.getIndiState().getOutput();
      saturated = lcs.getIndiState().getSaturated();
      break;
    case cereal::ControlsState::LateralControlState::LQR_STATE:
      cur_torque = lcs.getLqrState().getOutput();
      saturated = lcs.getLqrState().getSaturated();
      break;
    case cereal::ControlsState::LateralControlState::ANGLE_STATE:
      cur_torque = lcs.getAngleState().getOutput();
      saturated = lcs.getAngleState().getSaturated();
      break;
    default:
      break;
  }

  // Extract turn intent / lane change direction from lateralPlan
  int lc_dir = 0;
  if (sm.alive("lateralPlan") && sm.valid("lateralPlan")) {
    const auto lp = sm["lateralPlan"].getLateralPlan();
    if (lp.getLaneChangeState() != cereal::LateralPlan::LaneChangeState::OFF) {
      const auto dir = lp.getLaneChangeDirection();
      if (dir == cereal::LateralPlan::LaneChangeDirection::LEFT) {
        lc_dir = 1;
      } else if (dir == cereal::LateralPlan::LaneChangeDirection::RIGHT) {
        lc_dir = 2;
      }
    }
  }

  bool critical = (cs.getAlertStatus() == cereal::ControlsState::AlertStatus::CRITICAL);

  // Extract setDistance from cruiseState (1 = Aggressive, 2 = Normal, 3 = Chill, 4 = Auto)
  int dist_bars = 3;
  auto set_dist = sm["carState"].getCarState().getCruiseState().getSetDistance();
  switch (set_dist) {
    case cereal::CarState::CruiseState::SetDistance::AGGRESIVE:
      dist_bars = 1;
      break;
    case cereal::CarState::CruiseState::SetDistance::NORMAL:
      dist_bars = 2;
      break;
    case cereal::CarState::CruiseState::SetDistance::CHILL:
      dist_bars = 3;
      break;
    case cereal::CarState::CruiseState::SetDistance::AUTO:
      dist_bars = 4;
      break;
    default:
      dist_bars = 3;
      break;
  }

  // Extract lead vehicle tracking distance
  float lead_dist = 0.0f;
  bool lead_detected = false;
  if (sm.alive("radarState") && sm.valid("radarState")) {
    const auto lead_one = sm["radarState"].getRadarState().getLeadOne();
    if (lead_one.getStatus()) {
      lead_detected = true;
      lead_dist = lead_one.getDRel();
    }
  }

  if (getenv("FORCE_ONROAD") != NULL && cur_speed == 0.0) {
    cur_speed = 78.0;
    maxspeed_str = "80";
    cruise_set = true;
    temp = 28.0;
    steer_angle = -4.5f;
    steer_override = false;
    lat_active = true;
    thermal_st = 0;
    cur_torque = 0.38f;
    saturated = false;
    lc_dir = 0;
    critical = false;
    dist_bars = 3;
    lead_detected = true;
    lead_dist = 36.5f;
  }

  setProperty("is_cruise_set", cruise_set);
  setProperty("speed", QString::number(std::nearbyint(cur_speed)));
  setProperty("maxSpeed", maxspeed_str);
  setProperty("speedUnit", s.scene.is_metric ? "km/h" : "mph");
  setProperty("temperature", QString::number(std::nearbyint(temp)) + (s.scene.is_metric ? "°C" : "°F"));
  setProperty("steerAngleDeg", steer_angle);
  setProperty("steerOverride", steer_override);
  setProperty("lateralActive", lat_active);
  setProperty("steerTorque", cur_torque);
  setProperty("steerSaturated", saturated);
  setProperty("laneChangeDirection", lc_dir);
  setProperty("wheelCritical", critical);
  setProperty("distanceBars", dist_bars);
  setProperty("leadDistance", lead_dist);
  setProperty("hasLead", lead_detected);
  setProperty("thermalStatus", thermal_st);
  setProperty("hideDM", cs.getAlertSize() != cereal::ControlsState::AlertSize::NONE);
  setProperty("status", s.status);

  // update engageability and DM icons at 2Hz
  if (getenv("FORCE_ONROAD") != NULL) {
    setProperty("engageable", true);
    setProperty("dmActive", true);
  } else if (sm.frame % (UI_FREQ / 2) == 0) {
    setProperty("engageable", cs.getEngageable() || cs.getEnabled());
    setProperty("dmActive", sm["driverMonitoringState"].getDriverMonitoringState().getIsActiveMode());
  }
}

void OnroadHud::manualMouseEvent(QMouseEvent *e) {
  // make dm icon to act as settings button
  auto const x = radius / 2 + (bdr_s * 2);
  auto const y = rect().bottom() - footer_h / 2;
  auto const w = img_size;
  if (e->globalX() > x && e->globalY() > y && e->globalX() <= x + w && e->globalY() <= y + w) {
    emit openSettings();
    return;
  }

  // Tap MAX Speed capsule to cycle distance gap (1 -> 2 -> 3 -> 1)
  int set_speed_w = 200;
  QRect max_rc(bdr_s * 2, bdr_s * 1.5, set_speed_w, 204);
  if (max_rc.contains(e->pos())) {
    int next_bars = (distanceBars % 3) + 1;
    setProperty("distanceBars", next_bars);
  }
}

void OnroadHud::drawCapsule(QPainter &p, const QRect &rc) {
  p.setPen(QPen(QColor(255, 255, 255, 75), 5));
  p.setBrush(QColor(0, 0, 0, 166));
  p.drawRoundedRect(rc, 32, 32);
}

void OnroadHud::drawSetSpeedBox(QPainter &p, const QRect &rc) {
  drawCapsule(p, rc);

  // Exact release-mici MAX colors: ENGAGED = #80D8A6, OVERRIDE/DISENGAGED = #919B95, UNSET = #A6A6A6
  QColor max_color = QColor(166, 166, 166);
  if (is_cruise_set) {
    if (status == STATUS_ENGAGED) {
      max_color = QColor(128, 216, 166);
    } else if (status == STATUS_WARNING || steerOverride) {
      max_color = QColor(245, 158, 11);
    } else {
      max_color = QColor(145, 155, 149);
    }
  }

  configFont(p, "Inter", 38, "SemiBold");
  p.setPen(max_color);
  drawText(p, rc.center().x(), rc.top() + 44, "MAX", 255);

  if (is_cruise_set) {
    configFont(p, "Inter", 82, "Bold");
    p.setPen(QColor(255, 255, 255));
    drawText(p, rc.center().x(), rc.top() + 124, maxSpeed, 255);
  } else {
    configFont(p, "Inter", 78, "Bold");
    p.setPen(QColor(114, 114, 114));
    drawText(p, rc.center().x(), rc.top() + 124, "–", 200);
  }

  // Draw 3-bar distance gap indicator at bottom of card
  drawDistanceBars(p, rc.center().x(), rc.bottom() - 22, distanceBars, max_color);
}

void OnroadHud::drawDistanceBars(QPainter &p, int cx, int y, int bars, const QColor &active_color) {
  const int num_bars = 3;
  const int bar_w = 34;
  const int bar_h = 6;
  const int spacing = 8;
  const int total_w = num_bars * bar_w + (num_bars - 1) * spacing;
  int start_x = cx - total_w / 2;

  p.setPen(Qt::NoPen);

  // If a lead vehicle is tracked, display distance in meters (e.g. "36m") just above the bars
  if (hasLead && leadDistance > 0.5f) {
    QString lead_str = QString::number(std::round(leadDistance)) + "m";
    configFont(p, "Inter", 22, "SemiBold");
    p.setPen(QColor(255, 255, 255, 180));
    drawText(p, cx, y - 9, lead_str, 180);
    p.setPen(Qt::NoPen);
  }

  for (int i = 0; i < num_bars; ++i) {
    int bar_x = start_x + i * (bar_w + spacing);
    QRect bar_rc(bar_x, y, bar_w, bar_h);

    bool is_lit = (i < bars);
    if (is_lit) {
      p.setBrush(active_color);
    } else {
      p.setBrush(QColor(255, 255, 255, 45));
    }
    p.drawRoundedRect(bar_rc, 3, 3);
  }
}

void OnroadHud::drawCurrentSpeed(QPainter &p, int cx, int y) {
  configFont(p, "Inter", 176, "Bold");
  p.setPen(QColor(255, 255, 255));
  drawText(p, cx, y, speed, 255);

  configFont(p, "Inter", 54, "Medium");
  p.setPen(QColor(255, 255, 255, 200));
  drawText(p, cx, y + 80, speedUnit, 200);
}

void OnroadHud::drawTorqueArcBar(QPainter &p, int cx, int cy, int arc_radius, float torque, bool saturated) {
  QRectF arcRect(cx - arc_radius, cy - arc_radius, arc_radius * 2, arc_radius * 2);

  // Background subtle track (top 140° arc, centered at 90° / 12 o'clock)
  const int fullSpanDeg = 140;
  int bgStart = (90 + fullSpanDeg / 2) * 16;
  int bgSpan = -fullSpanDeg * 16;

  p.setBrush(Qt::NoBrush);
  p.setPen(QPen(QColor(255, 255, 255, 45), 5, Qt::SolidLine, Qt::RoundCap));
  p.drawArc(arcRect, bgStart, bgSpan);

  // Center reference tick
  p.setPen(QPen(QColor(255, 255, 255, 120), 3, Qt::SolidLine, Qt::RoundCap));
  p.drawArc(arcRect, 90 * 16 - 8, 16);

  // Active torque sweep: sweeps from 90° (top) toward left or right
  float clamped = std::clamp(torque, -1.0f, 1.0f);
  if (std::abs(clamped) > 0.02f) {
    float sweepAngle = -clamped * (fullSpanDeg / 2.0f);
    int activeStart = 90 * 16;
    int activeSpan = (int)std::round(sweepAngle * 16.0f);

    QColor torqueColor = QColor(255, 255, 255, 230);
    float absTorque = std::abs(clamped);
    if (saturated || absTorque > 0.85f) {
      torqueColor = QColor(248, 113, 113); // alert coral
    } else if (absTorque > 0.60f) {
      torqueColor = QColor(245, 158, 11);  // amber warning
    } else if (lateralActive) {
      torqueColor = QColor(128, 216, 166); // mint green
    }

    p.setPen(QPen(torqueColor, 6, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(arcRect, activeStart, activeSpan);
  }
}

void OnroadHud::drawMiciSteeringWheel(QPainter &p, int cx, int cy, float angle, bool critical) {
  p.save();
  p.translate(cx, cy);
  p.rotate(-angle);

  const QPixmap &wheel = critical ? wheel_critical_img : wheel_img;
  if (!wheel.isNull()) {
    p.setOpacity(lateralActive ? 1.0 : (status == STATUS_DISENGAGED ? 0.45 : 0.85));
    p.drawPixmap(-wheel.width() / 2, -wheel.height() / 2, wheel);
    p.setOpacity(1.0);
  }
  p.restore();

  // Exclamation mark if critical
  if (critical && !exclamation_img.isNull()) {
    p.drawPixmap(cx + 42, cy - exclamation_img.height() / 2, exclamation_img);
  }
}

void OnroadHud::drawTurnIntent(QPainter &p, int cx, int cy, int dir) {
  if (dir == 0 || turn_intent_img.isNull()) return;

  p.save();
  if (dir == 1) { // Left
    p.translate(cx - 56, cy);
    p.drawPixmap(-turn_intent_img.width() / 2, -turn_intent_img.height() / 2, turn_intent_img);
  } else if (dir == 2) { // Right (mirrored horizontally)
    p.translate(cx + 56, cy);
    p.scale(-1, 1);
    p.drawPixmap(-turn_intent_img.width() / 2, -turn_intent_img.height() / 2, turn_intent_img);
  }
  p.restore();
}

void OnroadHud::drawStatusCapsule(QPainter &p, const QRect &rc, const QString &label, const QString &val, const QColor &color) {
  drawCapsule(p, rc);

  configFont(p, "Inter", 38, "SemiBold");
  p.setPen(QColor(166, 166, 166));
  drawText(p, rc.center().x(), rc.top() + 48, label, 255);

  configFont(p, "Inter", 64, "Bold");
  p.setPen(color);
  drawText(p, rc.center().x(), rc.top() + 142, val, 255);
}

void OnroadHud::drawActionBtn(QPainter &p, int x, int y, QPixmap &img, bool active) {
  p.setPen(QPen(QColor(255, 255, 255, 40), 2));
  p.setBrush(QColor(18, 22, 30, active ? 180 : 120));
  p.drawEllipse(x - radius / 2, y - radius / 2, radius, radius);
  p.setOpacity(active ? 1.0 : 0.4);
  p.drawPixmap(x - img_size / 2, y - img_size / 2, img);
  p.setOpacity(1.0);
}

void OnroadHud::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  // 1. Release-mici header vertical gradient (300px)
  QLinearGradient bg(0, 0, 0, 320);
  bg.setColorAt(0, QColor(0, 0, 0, 150));
  bg.setColorAt(0.65, QColor(0, 0, 0, 65));
  bg.setColorAt(1, QColor(0, 0, 0, 0));
  p.fillRect(0, 0, width(), 320, bg);

  // 2. MAX Speed box (top-left) - 200x204
  int set_speed_w = 200;
  QRect max_rc(bdr_s * 2, bdr_s * 1.5, set_speed_w, 204);
  drawSetSpeedBox(p, max_rc);

  // 3. STEER & TORQUE Gauge box (next to MAX speed) - 200x204
  QRect steer_rc(bdr_s * 2 + set_speed_w + 24, bdr_s * 1.5, set_speed_w, 204);
  drawCapsule(p, steer_rc);

  configFont(p, "Inter", 38, "SemiBold");
  QColor steer_lbl_col = lateralActive ? QColor(128, 216, 166) : (steerOverride ? QColor(245, 158, 11) : QColor(166, 166, 166));
  p.setPen(steer_lbl_col);
  drawText(p, steer_rc.center().x(), steer_rc.top() + 48, "STEER", 255);

  // Draw dynamic torque arc bar around wheel
  int wheel_cx = steer_rc.center().x();
  int wheel_cy = steer_rc.top() + 115;
  drawTorqueArcBar(p, wheel_cx, wheel_cy, 46, steerTorque, steerSaturated);

  // Draw release-mici rotating steering wheel
  drawMiciSteeringWheel(p, wheel_cx, wheel_cy, steerAngleDeg, wheelCritical);

  // Draw lane change turn intent indicator
  drawTurnIntent(p, wheel_cx, wheel_cy, laneChangeDirection);

  // Digital angle below wheel
  QString angle_str = (steerAngleDeg > 0.0f ? "+" : "") + QString::number(std::round(steerAngleDeg)) + "°";
  configFont(p, "Inter", 32, "SemiBold");
  p.setPen(steer_lbl_col);
  drawText(p, wheel_cx, steer_rc.bottom() - 14, angle_str, 255);

  // 4. Current Speed (top-center)
  drawCurrentSpeed(p, rect().center().x(), 180);

  // 5. TEMP / Status capsule (top-right) - 180x204
  int temp_w = 180;
  QRect temp_rc(rect().right() - bdr_s * 2 - temp_w, bdr_s * 1.5, temp_w, 204);
  QColor temp_col = QColor(240, 243, 246);
  if (thermalStatus == (int)cereal::DeviceState::ThermalStatus::YELLOW) {
    temp_col = QColor(251, 191, 36);
  } else if (thermalStatus >= (int)cereal::DeviceState::ThermalStatus::RED) {
    temp_col = QColor(248, 113, 113);
  }
  drawStatusCapsule(p, temp_rc, "TEMP", temperature, temp_col);

  // 6. Bottom floating action discs (DM & Settings)
  if (!hideDM) {
    drawActionBtn(p, rect().right() - radius / 2 - (bdr_s * 2), rect().bottom() - footer_h / 2,
                  dm_img, dmActive);
  }

  drawActionBtn(p, radius / 2 + (bdr_s * 2), rect().bottom() - footer_h / 2,
                settings_img, engageable);
}

void OnroadHud::drawText(QPainter &p, int x, int y, const QString &text, int alpha) {
  QFontMetrics fm(p.font());
  QRect init_rect = fm.boundingRect(text);
  QRect real_rect = fm.boundingRect(init_rect, 0, text);
  real_rect.moveCenter({x, y - real_rect.height() / 2});

  p.setPen(QColor(0xff, 0xff, 0xff, alpha));
  p.drawText(real_rect.x(), real_rect.bottom(), text);
}

void OnroadHud::drawIcon(QPainter &p, int x, int y, QPixmap &img, QBrush bg, float opacity) {
  p.setPen(Qt::NoPen);
  p.setBrush(bg);
  p.drawEllipse(x - radius / 2, y - radius / 2, radius, radius);
  p.setOpacity(opacity);
  p.drawPixmap(x - img_size / 2, y - img_size / 2, img);
}

// NvgWindow
void NvgWindow::initializeGL() {
  CameraViewWidget::initializeGL();
  qInfo() << "OpenGL version:" << QString((const char*)glGetString(GL_VERSION));
  qInfo() << "OpenGL vendor:" << QString((const char*)glGetString(GL_VENDOR));
  qInfo() << "OpenGL renderer:" << QString((const char*)glGetString(GL_RENDERER));
  qInfo() << "OpenGL language version:" << QString((const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));

  prev_draw_t = millis_since_boot();
  setBackgroundColor(bg_colors[STATUS_DISENGAGED]);
}

void NvgWindow::updateFrameMat(int w, int h) {
  CameraViewWidget::updateFrameMat(w, h);

  UIState *s = uiState();
  s->fb_w = w;
  s->fb_h = h;
  auto intrinsic_matrix = s->wide_camera ? ecam_intrinsic_matrix : fcam_intrinsic_matrix;
  float zoom = ZOOM / intrinsic_matrix.v[0];
  if (s->wide_camera) {
    zoom *= 0.5;
  }
  // Apply transformation such that video pixel coordinates match video
  // 1) Put (0, 0) in the middle of the video
  // 2) Apply same scaling as video
  // 3) Put (0, 0) in top left corner of video
  s->car_space_transform.reset();
  s->car_space_transform.translate(w / 2, h / 2 + y_offset)
      .scale(zoom, zoom)
      .translate(-intrinsic_matrix.v[2], -intrinsic_matrix.v[5]);
}

void NvgWindow::drawLaneLines(QPainter &painter, const UIScene &scene) {
  if (!scene.end_to_end) {
    // lanelines
    for (int i = 0; i < std::size(scene.lane_line_vertices); ++i) {
      painter.setBrush(QColor::fromRgbF(1.0, 1.0, 1.0, scene.lane_line_probs[i]));
      painter.drawPolygon(scene.lane_line_vertices[i].v, scene.lane_line_vertices[i].cnt);
    }
    // road edges
    for (int i = 0; i < std::size(scene.road_edge_vertices); ++i) {
      painter.setBrush(QColor::fromRgbF(1.0, 0, 0, std::clamp<float>(1.0 - scene.road_edge_stds[i], 0.0, 1.0)));
      painter.drawPolygon(scene.road_edge_vertices[i].v, scene.road_edge_vertices[i].cnt);
    }
  }
  // paint path
  QLinearGradient bg(0, height(), 0, height() / 4);
  bg.setColorAt(0, scene.end_to_end ? redColor() : QColor(255, 255, 255));
  bg.setColorAt(1, scene.end_to_end ? redColor(0) : QColor(255, 255, 255, 0));
  painter.setBrush(bg);
  painter.drawPolygon(scene.track_vertices.v, scene.track_vertices.cnt);
}

void NvgWindow::drawLead(QPainter &painter, const cereal::ModelDataV2::LeadDataV3::Reader &lead_data, const QPointF &vd) {
  const float speedBuff = 10.;
  const float leadBuff = 40.;
  const float d_rel = lead_data.getX()[0];
  const float v_rel = lead_data.getV()[0];

  float fillAlpha = 0;
  if (d_rel < leadBuff) {
    fillAlpha = 255 * (1.0 - (d_rel / leadBuff));
    if (v_rel < 0) {
      fillAlpha += 255 * (-1 * (v_rel / speedBuff));
    }
    fillAlpha = (int)(fmin(fillAlpha, 255));
  }

  float sz = std::clamp((25 * 30) / (d_rel / 3 + 30), 15.0f, 30.0f) * 2.35;
  float x = std::clamp((float)vd.x(), 0.f, width() - sz / 2);
  float y = std::fmin(height() - sz * .6, (float)vd.y());

  float g_xo = sz / 5;
  float g_yo = sz / 10;

  QPointF glow[] = {{x + (sz * 1.35) + g_xo, y + sz + g_yo}, {x, y - g_yo}, {x - (sz * 1.35) - g_xo, y + sz + g_yo}};
  painter.setBrush(QColor(218, 202, 37, 255));
  painter.drawPolygon(glow, std::size(glow));

  // chevron
  QPointF chevron[] = {{x + (sz * 1.25), y + sz}, {x, y}, {x - (sz * 1.25), y + sz}};
  painter.setBrush(redColor(fillAlpha));
  painter.drawPolygon(chevron, std::size(chevron));
}

void NvgWindow::paintGL() {
  CameraViewWidget::paintGL();

  UIState *s = uiState();
  if (s->worldObjectsVisible()) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    drawLaneLines(painter, s->scene);

    if (s->scene.longitudinal_control) {
      auto leads = (*s->sm)["modelV2"].getModelV2().getLeadsV3();
      if (leads[0].getProb() > .5) {
        drawLead(painter, leads[0], s->scene.lead_vertices[0]);
      }
      if (leads[1].getProb() > .5 && (std::abs(leads[1].getX()[0] - leads[0].getX()[0]) > 3.0)) {
        drawLead(painter, leads[1], s->scene.lead_vertices[1]);
      }
    }
  }

  double cur_draw_t = millis_since_boot();
  double dt = cur_draw_t - prev_draw_t;
  if (dt > 66) {
    // warn on sub 15fps
    LOGW("slow frame time: %.2f", dt);
  }
  prev_draw_t = cur_draw_t;
}

void NvgWindow::showEvent(QShowEvent *event) {
  CameraViewWidget::showEvent(event);

  ui_update_params(uiState());
  prev_draw_t = millis_since_boot();
}
