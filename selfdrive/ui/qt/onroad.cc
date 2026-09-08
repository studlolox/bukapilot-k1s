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

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::TextAntialiasing);

  if (alert.size == cereal::ControlsState::AlertSize::SMALL) {
    // 1. SMALL ALERT: Modern floating capsule pill
    int pill_w = std::min(width() - 200, 1300);
    int pill_h = 160;
    int pill_x = (width() - pill_w) / 2;
    int pill_y = height() - footer_h + 10;
    QRect pill_rc(pill_x, pill_y, pill_w, pill_h);

    // Frosted obsidian glass background with vibrant accent border
    p.setPen(QPen(QColor(bg.red(), bg.green(), bg.blue(), 230), 4.5));
    p.setBrush(QColor(15, 20, 28, 230));
    p.drawRoundedRect(pill_rc, pill_h / 2, pill_h / 2);

    // Left accent status indicator dot
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(bg.red(), bg.green(), bg.blue(), 255));
    p.drawEllipse(QPointF(pill_x + 50, pill_rc.center().y()), 12, 12);

    // Alert text
    configFont(p, "Inter", 54, "Bold");
    p.setPen(QColor(255, 255, 255));
    p.drawText(pill_rc.adjusted(80, 0, -50, 0), Qt::AlignCenter, alert.text1);

  } else if (alert.size == cereal::ControlsState::AlertSize::MID) {
    // 2. MID ALERT: Modern floating centered card
    int card_w = std::min(width() - 140, 1500);
    int card_h = 280;
    int card_x = (width() - card_w) / 2;
    int card_y = height() - card_h - 40;
    QRect card_rc(card_x, card_y, card_w, card_h);

    // Frosted card with colored accent border
    p.setPen(QPen(QColor(bg.red(), bg.green(), bg.blue(), 235), 5));
    p.setBrush(QColor(15, 20, 28, 238));
    p.drawRoundedRect(card_rc, 36, 36);

    // Top accent gradient bar inside card
    QLinearGradient accent_bar(card_rc.left(), card_rc.top(), card_rc.right(), card_rc.top());
    accent_bar.setColorAt(0.0, QColor(bg.red(), bg.green(), bg.blue(), 0));
    accent_bar.setColorAt(0.5, QColor(bg.red(), bg.green(), bg.blue(), 220));
    accent_bar.setColorAt(1.0, QColor(bg.red(), bg.green(), bg.blue(), 0));
    p.setPen(Qt::NoPen);
    p.setBrush(accent_bar);
    p.drawRoundedRect(QRect(card_x + 36, card_y + 4, card_w - 72, 6), 3, 3);

    // Text 1 (Headline)
    configFont(p, "Inter", 66, "Bold");
    p.setPen(QColor(255, 255, 255));
    p.drawText(QRect(card_x + 40, card_y + 36, card_w - 80, 96), Qt::AlignHCenter | Qt::AlignTop, alert.text1);

    // Text 2 (Instruction / subtext)
    configFont(p, "Inter", 48, "Medium");
    p.setPen(QColor(209, 213, 219)); // Slate-300 silver
    p.drawText(QRect(card_x + 40, card_y + 144, card_w - 80, 110), Qt::AlignHCenter | Qt::TextWordWrap, alert.text2);

  } else if (alert.size == cereal::ControlsState::AlertSize::FULL) {
    // 3. FULL ALERT: Urgent full-screen takeover with dark radial vignette
    QRect full_rc = rect();

    QRadialGradient bg_vignette(full_rc.center().x(), full_rc.center().y(), full_rc.width() * 0.75);
    bg_vignette.setColorAt(0.0, QColor(bg.red(), bg.green(), bg.blue(), 220));
    bg_vignette.setColorAt(1.0, QColor(bg.red() / 2, bg.green() / 2, bg.blue() / 2, 250));
    p.setPen(Qt::NoPen);
    p.setBrush(bg_vignette);
    p.drawRect(full_rc);

    bool long_text = alert.text1.length() > 15;
    configFont(p, "Inter", long_text ? 110 : 140, "Bold");
    p.setPen(QColor(255, 255, 255));
    p.drawText(QRect(60, full_rc.top() + (long_text ? 220 : 250), width() - 120, 500), Qt::AlignHCenter | Qt::TextWordWrap, alert.text1);

    configFont(p, "Inter", 72, "Medium");
    p.setPen(QColor(255, 255, 255, 230));
    p.drawText(QRect(60, full_rc.height() - (long_text ? 360 : 420), width() - 120, 300), Qt::AlignHCenter | Qt::TextWordWrap, alert.text2);
  }
}

// OnroadHud
OnroadHud::OnroadHud(QWidget *parent) : QWidget(parent) {
  dm_img = loadPixmap("../assets/img_driver_face.png", {img_size, img_size});
  settings_img = loadPixmap("../assets/kommu/settings.png", {img_size, img_size});
  exp_img = loadPixmap("../assets/icons_mici/experimental.png", {img_size, img_size});
  chffr_wheel_img = loadPixmap("../assets/img_chffr_wheel.png", {img_size, img_size});
  wheel_img = loadPixmap("../assets/icons_mici/wheel.png", {110, 110});
  wheel_critical_img = loadPixmap("../assets/icons_mici/wheel_critical.png", {110, 110});
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

  // Extract model confidence from disengagePredictions
  float conf = 1.0f;
  if (sm.alive("modelV2") && sm.valid("modelV2")) {
    const auto meta = sm["modelV2"].getModelV2().getMeta();
    const auto dp = meta.getDisengagePredictions();
    float max_brake = 0.0f;
    for (float p_val : dp.getBrakeDisengageProbs()) {
      max_brake = std::max(max_brake, p_val);
    }
    float max_steer = 0.0f;
    for (float p_val : dp.getSteerOverrideProbs()) {
      max_steer = std::max(max_steer, p_val);
    }
    conf = (1.0f - max_brake) * (1.0f - max_steer);
    conf = std::clamp(conf, 0.0f, 1.0f);
  }

  if (getenv("FORCE_ONROAD") != NULL) {
    conf = 0.94f;
  }
  modelConfidence = 0.82f * modelConfidence + 0.18f * conf;

  // Extract driver monitoring awareness and head pose
  float dm_awareness = 1.0f;
  bool dm_distracted = false;
  bool dm_face_detected = false;
  float dm_yaw = 0.0f;
  float dm_pitch = 0.0f;

  if (sm.alive("driverMonitoringState") && sm.valid("driverMonitoringState")) {
    const auto dms = sm["driverMonitoringState"].getDriverMonitoringState();
    dm_awareness = std::clamp(dms.getAwarenessStatus(), 0.0f, 1.0f);
    dm_distracted = dms.getIsDistracted();
    dm_face_detected = dms.getFaceDetected();
  }

  if (sm.alive("driverState") && sm.valid("driverState")) {
    const auto ds = sm["driverState"].getDriverState();
    auto orient = ds.getFaceOrientation();
    if (orient.size() >= 3) {
      dm_pitch = orient[1] * (180.0f / (float)M_PI);
      dm_yaw = orient[2] * (180.0f / (float)M_PI);
    }
  }

  if (getenv("FORCE_ONROAD") != NULL) {
    dm_awareness = 0.95f;
    dm_distracted = false;
    dm_face_detected = true;
    dm_yaw = -3.5f;
    dm_pitch = 1.5f;
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
  setProperty("modelConfidence", modelConfidence);
  setProperty("experimental_mode", s.scene.end_to_end);
  setProperty("dmAwareness", dm_awareness);
  setProperty("dmDistracted", dm_distracted);
  setProperty("dmFaceDetected", dm_face_detected);
  setProperty("dmYaw", dm_yaw);
  setProperty("dmPitch", dm_pitch);
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
  QPoint pt = mapFromGlobal(e->globalPos());

  // Settings button (bottom-left)
  int settings_cx = radius / 2 + (bdr_s * 2);
  int settings_cy = rect().bottom() - footer_h / 2;
  if (std::hypot(pt.x() - settings_cx, pt.y() - settings_cy) <= radius / 2 ||
      (e->pos().x() >= settings_cx - radius / 2 && e->pos().x() <= settings_cx + radius / 2 &&
       e->pos().y() >= settings_cy - radius / 2 && e->pos().y() <= settings_cy + radius / 2)) {
    emit openSettings();
    return;
  }

  // Mode button (Laneless / E2E toggle, next to Settings button)
  int mode_cx = settings_cx + radius + 32;
  int mode_cy = settings_cy;
  if (std::hypot(pt.x() - mode_cx, pt.y() - mode_cy) <= radius / 2 ||
      (e->pos().x() >= mode_cx - radius / 2 && e->pos().x() <= mode_cx + radius / 2 &&
       e->pos().y() >= mode_cy - radius / 2 && e->pos().y() <= mode_cy + radius / 2)) {
    bool next_e2e = !experimental_mode;
    Params().putBool("EndToEndToggle", next_e2e);
    setProperty("experimental_mode", next_e2e);
    return;
  }

  // Tap MAX Speed capsule to cycle distance gap (1 -> 2 -> 3 -> 1)
  int set_speed_w = 200;
  QRect max_rc(bdr_s * 2, bdr_s * 1.5, set_speed_w, 204);
  if (max_rc.contains(pt) || max_rc.contains(e->pos())) {
    int next_bars = (distanceBars % 3) + 1;
    setProperty("distanceBars", next_bars);
    return;
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

void OnroadHud::drawBottomTorqueArcBar(QPainter &p, int cx, int y, float torque, bool saturated) {
  p.save();
  p.setRenderHint(QPainter::Antialiasing);

  const float R = 1100.0f;
  const float theta_max = 11.0f; // Half-span in degrees (total 22° span, ~420px wide)
  QRectF arcRect(cx - R, y, 2.0f * R, 2.0f * R);

  // 1. Background Track
  // Arc sweeps from 90° + theta_max (left) clockwise by -2 * theta_max to 90° - theta_max (right)
  int bgStart = (int)std::round((90.0f + theta_max) * 16.0f);
  int bgSpan = (int)std::round((-2.0f * theta_max) * 16.0f);

  // Outer subtle border / glow for track
  p.setBrush(Qt::NoBrush);
  p.setPen(QPen(QColor(255, 255, 255, 38), 24, Qt::SolidLine, Qt::RoundCap));
  p.drawArc(arcRect, bgStart, bgSpan);

  // Inner dark frosted obsidian glass track
  p.setPen(QPen(QColor(15, 20, 28, 210), 19, Qt::SolidLine, Qt::RoundCap));
  p.drawArc(arcRect, bgStart, bgSpan);

  // 2. Center Zero Reference Tick (at 90°, 12 o'clock)
  p.setPen(QPen(QColor(255, 255, 255, 115), 2.5, Qt::SolidLine, Qt::RoundCap));
  p.drawLine(QPointF(cx, y - 6.0f), QPointF(cx, y + 6.0f));

  // 3. Torque Dynamics Calculation
  float clamped = std::clamp(torque, -1.0f, 1.0f);
  float abs_torque = std::abs(clamped);

  const float pill_half_span = 2.6f; // ~100px pill length
  const float max_deflection = theta_max - pill_half_span - 0.5f; // ~7.9° max travel
  float pill_center_deg = 90.0f - clamped * max_deflection;

  // Colors based on torque magnitude & state
  QColor pill_core_col = QColor(255, 255, 255, 250);
  QColor pill_glow_col = QColor(255, 255, 255, 75);
  QColor trail_col = QColor(255, 255, 255, 95);

  if (saturated || abs_torque > 0.85f) {
    pill_core_col = QColor(248, 113, 113, 255); // Alert coral red
    pill_glow_col = QColor(248, 113, 113, 110);
    trail_col = QColor(248, 113, 113, 170);
  } else if (abs_torque > 0.60f) {
    pill_core_col = QColor(245, 158, 11, 255);  // Warning amber
    pill_glow_col = QColor(245, 158, 11, 100);
    trail_col = QColor(245, 158, 11, 160);
  } else if (lateralActive) {
    pill_core_col = QColor(255, 255, 255, 255);
    pill_glow_col = QColor(255, 255, 255, 80);
    trail_col = QColor(128, 216, 166, 140);      // Subtle mint connection trail
  } else {
    pill_core_col = QColor(215, 220, 228, 190);
    pill_glow_col = QColor(255, 255, 255, 30);
    trail_col = QColor(255, 255, 255, 50);
  }

  // 4. Dynamic Force Sweep Trail (between 90° center and pill center)
  if (abs_torque > 0.025f) {
    float trail_span_deg = -clamped * max_deflection;
    int trailStart = 90 * 16;
    int trailSpan = (int)std::round(trail_span_deg * 16.0f);
    p.setPen(QPen(trail_col, 10, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(arcRect, trailStart, trailSpan);
  }

  // 5. Active Slider Pill Capsule
  int pillStart = (int)std::round((pill_center_deg + pill_half_span) * 16.0f);
  int pillSpan = (int)std::round((-2.0f * pill_half_span) * 16.0f);

  // Soft luminous outer glow around slider pill
  p.setPen(QPen(pill_glow_col, 20, Qt::SolidLine, Qt::RoundCap));
  p.drawArc(arcRect, pillStart, pillSpan);

  // Solid bright core of slider pill
  p.setPen(QPen(pill_core_col, 14, Qt::SolidLine, Qt::RoundCap));
  p.drawArc(arcRect, pillStart, pillSpan);

  p.restore();
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
    p.drawPixmap(cx + 52, cy - exclamation_img.height() / 2, exclamation_img);
  }
}

void OnroadHud::drawTurnIntent(QPainter &p, int cx, int cy, int dir) {
  if (dir == 0 || turn_intent_img.isNull()) return;

  p.save();
  if (dir == 1) { // Left
    p.translate(cx - 68, cy);
    p.drawPixmap(-turn_intent_img.width() / 2, -turn_intent_img.height() / 2, turn_intent_img);
  } else if (dir == 2) { // Right (mirrored horizontally)
    p.translate(cx + 68, cy);
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

void OnroadHud::drawModeBtn(QPainter &p, int x, int y, bool is_experimental) {
  p.save();
  const int btn_r = radius / 2;
  QRect btn_rc(x - btn_r, y - btn_r, radius, radius);

  if (is_experimental) {
    // Release-mici glowing amber flame disc for E2E / Experimental Mode
    QRadialGradient glow(x, y, btn_r);
    glow.setColorAt(0.0, QColor(245, 158, 11, 160));
    glow.setColorAt(0.75, QColor(217, 119, 6, 100));
    glow.setColorAt(1.0, QColor(180, 83, 9, 30));
    p.setBrush(glow);
    p.setPen(QPen(QColor(245, 158, 11, 230), 4));
    p.drawEllipse(btn_rc);

    if (!exp_img.isNull()) {
      p.drawPixmap(x - exp_img.width() / 2, y - exp_img.height() / 2, exp_img);
    }
  } else {
    // Standard mode: Clean dark glass disc with chffr steering wheel
    p.setBrush(QColor(18, 22, 30, 150));
    p.setPen(QPen(QColor(255, 255, 255, 45), 2));
    p.drawEllipse(btn_rc);

    if (!chffr_wheel_img.isNull()) {
      p.setOpacity(0.8);
      p.drawPixmap(x - chffr_wheel_img.width() / 2, y - chffr_wheel_img.height() / 2, chffr_wheel_img);
      p.setOpacity(1.0);
    }
  }

  // Label underneath: "LANES" or "E2E"
  configFont(p, "Inter", 22, "SemiBold");
  p.setPen(is_experimental ? QColor(245, 158, 11) : QColor(180, 190, 200, 200));
  drawText(p, x, y + btn_r - 12, is_experimental ? "E2E" : "LANES", 220);

  p.restore();
}

void OnroadHud::drawDriverMonitoringDisc(QPainter &p, int x, int y, bool active, float awareness, bool distracted, bool face_detected, float yaw, float pitch) {
  p.save();
  const int btn_r = radius / 2;
  QRect btn_rc(x - btn_r, y - btn_r, radius, radius);

  // 1. Base dark glass disc
  p.setBrush(QColor(18, 22, 30, active ? 180 : 120));
  p.setPen(QPen(QColor(255, 255, 255, 40), 2));
  p.drawEllipse(btn_rc);

  // 2. Awareness arc ring around the disc
  if (active) {
    const int ring_r = btn_r + 6;
    QRectF ring_rc(x - ring_r, y - ring_r, ring_r * 2, ring_r * 2);

    // Subtle track
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 25), 4, Qt::SolidLine, Qt::RoundCap));
    p.drawEllipse(ring_rc);

    // Sweep arc proportional to awareness (360 deg, clockwise from top)
    float span = std::clamp(awareness, 0.0f, 1.0f) * 360.0f;
    int start_angle = 90 * 16;
    int span_angle = (int)(-span * 16.0f);

    QColor ring_col;
    if (distracted || awareness < 0.35f) {
      ring_col = QColor(248, 113, 113); // Coral alert
    } else if (awareness < 0.70f) {
      ring_col = QColor(245, 158, 11);  // Amber warning
    } else {
      ring_col = QColor(128, 216, 166); // Emerald / mint
    }

    p.setPen(QPen(ring_col, 5, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(ring_rc, start_angle, span_angle);

    // If distracted: outer warning pulse halo
    if (distracted) {
      p.setPen(QPen(QColor(248, 113, 113, 90), 10, Qt::SolidLine, Qt::RoundCap));
      p.drawArc(ring_rc, start_angle, span_angle);
    }
  }

  // 3. Driver face icon
  if (!dm_img.isNull()) {
    p.setOpacity(active && face_detected ? 1.0 : (active ? 0.6 : 0.35));
    p.drawPixmap(x - img_size / 2, y - img_size / 2, dm_img);
    p.setOpacity(1.0);
  }

  // 4. Gaze offset indicator dot (head pose tracking)
  if (active && face_detected) {
    float norm_yaw = std::clamp(yaw / 30.0f, -1.0f, 1.0f);
    float norm_pitch = std::clamp(pitch / 20.0f, -1.0f, 1.0f);
    float gaze_x = x + norm_yaw * 24.0f;
    float gaze_y = y - norm_pitch * 20.0f;

    QColor gaze_col = distracted ? QColor(248, 113, 113) : QColor(0, 245, 212);
    p.setBrush(gaze_col);
    p.setPen(QPen(QColor(255, 255, 255, 200), 1.5));
    p.drawEllipse(QPointF(gaze_x, gaze_y), 4.5f, 4.5f);
  }

  p.restore();
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

  // 3. STEER Box (next to MAX speed) - 200x204
  QRect steer_rc(bdr_s * 2 + set_speed_w + 24, bdr_s * 1.5, set_speed_w, 204);
  drawCapsule(p, steer_rc);

  configFont(p, "Inter", 38, "SemiBold");
  QColor steer_lbl_col = lateralActive ? QColor(128, 216, 166) : (steerOverride ? QColor(245, 158, 11) : QColor(166, 166, 166));
  p.setPen(steer_lbl_col);
  drawText(p, steer_rc.center().x(), steer_rc.top() + 48, "STEER", 255);

  // Draw enlarged rotating steering wheel (centered with angle text removed)
  int wheel_cx = steer_rc.center().x();
  int wheel_cy = steer_rc.top() + 124;
  drawMiciSteeringWheel(p, wheel_cx, wheel_cy, steerAngleDeg, wheelCritical);

  // Draw lane change turn intent indicator
  drawTurnIntent(p, wheel_cx, wheel_cy, laneChangeDirection);

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

  // 6. Dynamic Torque Arc Bar at bottom center like Comma 4
  drawBottomTorqueArcBar(p, rect().center().x(), rect().bottom() - 110, steerTorque, steerSaturated);

  // 7. Bottom floating action discs (DM, Settings, & Mode)
  if (!hideDM) {
    int dm_cx = rect().right() - radius / 2 - (bdr_s * 2);
    int dm_cy = rect().bottom() - footer_h / 2;
    drawDriverMonitoringDisc(p, dm_cx, dm_cy, dmActive, dmAwareness, dmDistracted, dmFaceDetected, dmYaw, dmPitch);
  }

  int settings_cx = radius / 2 + (bdr_s * 2);
  int settings_cy = rect().bottom() - footer_h / 2;
  drawActionBtn(p, settings_cx, settings_cy, settings_img, engageable);

  int mode_cx = settings_cx + radius + 32;
  int mode_cy = settings_cy;
  drawModeBtn(p, mode_cx, mode_cy, experimental_mode);

  // 8. Right-edge AI Confidence Ball
  int ball_x = rect().right() - bdr_s / 2 - 4;
  int ball_top_y = 120;
  int ball_bot_y = rect().bottom() - footer_h - 10;
  drawConfidenceBall(p, ball_x, ball_top_y, ball_bot_y, modelConfidence);
}

void OnroadHud::drawConfidenceBall(QPainter &p, int x, int top_y, int bottom_y, float confidence) {
  int total_h = bottom_y - top_y;
  float clamped = std::clamp(confidence, 0.0f, 1.0f);
  float ball_y = (1.0f - clamped) * total_h + top_y;
  const float r = 16.0f;

  // 1. Subtle vertical tracking guide line
  p.setPen(QPen(QColor(255, 255, 255, 30), 2, Qt::SolidLine, Qt::RoundCap));
  p.drawLine(x, top_y, x, bottom_y);

  // 2. Determine confidence gradient colors matching release-mici
  QColor top_col, bot_col;
  if (status == STATUS_ENGAGED || getenv("FORCE_ONROAD") != NULL) {
    if (clamped > 0.5f) {
      // High confidence: Neon cyan to emerald green
      top_col = QColor(0, 255, 204);
      bot_col = QColor(0, 255, 38);
    } else if (clamped > 0.2f) {
      // Moderate confidence: Amber to orange
      top_col = QColor(255, 200, 0);
      bot_col = QColor(255, 115, 0);
    } else {
      // Low confidence: Alert coral to red
      top_col = QColor(255, 0, 21);
      bot_col = QColor(255, 0, 89);
    }
  } else if (steerOverride) {
    top_col = QColor(255, 255, 255);
    bot_col = QColor(100, 100, 100);
  } else {
    // Disengaged
    top_col = QColor(75, 75, 75);
    bot_col = QColor(30, 30, 30);
  }

  // 3. Outer soft glow
  if (status == STATUS_ENGAGED || getenv("FORCE_ONROAD") != NULL) {
    QRadialGradient glow(x, ball_y, r * 2.2f);
    glow.setColorAt(0.0, QColor(top_col.red(), top_col.green(), top_col.blue(), 110));
    glow.setColorAt(1.0, QColor(top_col.red(), top_col.green(), top_col.blue(), 0));
    p.setBrush(glow);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(x, ball_y), r * 2.2f, r * 2.2f);
  }

  // 4. Core Confidence Ball
  QLinearGradient core(x, ball_y - r, x, ball_y + r);
  core.setColorAt(0.0, top_col);
  core.setColorAt(1.0, bot_col);
  p.setBrush(core);
  p.setPen(QPen(QColor(255, 255, 255, 160), 2.0));
  p.drawEllipse(QPointF(x, ball_y), r, r);
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

QLinearGradient NvgWindow::getPathGradient(const UIScene &scene, float a_ego, bool engaged) {
  QLinearGradient bg(0, height(), 0, height() / 4);

  if (scene.end_to_end) {
    bg.setColorAt(0.0, redColor(180));
    bg.setColorAt(1.0, redColor(0));
    return bg;
  }

  if (!engaged) {
    // Disengaged: subtle platinum white / slate
    bg.setColorAt(0.0, QColor(210, 215, 220, 120));
    bg.setColorAt(1.0, QColor(210, 215, 220, 0));
    return bg;
  }

  // Release-mici throttle vs decel dynamic gradient
  if (a_ego >= 0.05f) {
    // Accelerating / Cruising under throttle: emerald mint glow
    bg.setColorAt(0.0, QColor(13, 248, 122, 175));
    bg.setColorAt(0.45, QColor(114, 255, 92, 115));
    bg.setColorAt(1.0, QColor(114, 255, 92, 0));
  } else if (a_ego >= -0.6f) {
    // Coasting / Steady state: sleek platinum white
    bg.setColorAt(0.0, QColor(242, 242, 242, 160));
    bg.setColorAt(0.5, QColor(242, 242, 242, 85));
    bg.setColorAt(1.0, QColor(242, 242, 242, 0));
  } else {
    // Decelerating / Braking: alert amber & coral glow
    bg.setColorAt(0.0, QColor(245, 158, 11, 180));
    bg.setColorAt(0.45, QColor(239, 68, 68, 110));
    bg.setColorAt(1.0, QColor(239, 68, 68, 0));
  }

  return bg;
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

  UIState *s = uiState();
  float a_ego = 0.0f;
  if (s->sm->alive("carState") && s->sm->valid("carState")) {
    a_ego = (*s->sm)["carState"].getCarState().getAEgo();
  }
  if (getenv("FORCE_ONROAD") != NULL && a_ego == 0.0f) {
    a_ego = 0.25f;
  }

  // Paint dynamic path with release-mici gradient
  painter.setBrush(getPathGradient(scene, a_ego, s->engaged() || getenv("FORCE_ONROAD") != NULL));
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

  // 1. Dynamic Radar Glow Aura
  float glow_r = sz * (v_rel < -1.0f ? 2.4f : 1.8f);
  QRadialGradient rad_glow(x, y + sz * 0.5f, glow_r);
  QColor aura_color = (v_rel < -1.5f || d_rel < 15.0f) ? QColor(248, 113, 113) : QColor(251, 191, 36);
  rad_glow.setColorAt(0.0, QColor(aura_color.red(), aura_color.green(), aura_color.blue(), (int)(fillAlpha * 0.75f)));
  rad_glow.setColorAt(1.0, QColor(aura_color.red(), aura_color.green(), aura_color.blue(), 0));
  painter.setBrush(rad_glow);
  painter.drawEllipse(QPointF(x, y + sz * 0.5f), glow_r, glow_r * 0.65f);

  // 2. Modern Aerodynamic Chevron Radar Target
  QPointF chevron[] = {
    {x, y},
    {x + sz * 1.35f, y + sz},
    {x, y + sz * 0.45f},
    {x - sz * 1.35f, y + sz}
  };

  QColor chevron_fill = (fillAlpha > 120) ? redColor(fillAlpha) : QColor(255, 255, 255, (int)(fillAlpha * 0.85f));
  painter.setBrush(chevron_fill);
  painter.setPen(QPen(QColor(255, 255, 255, 230), 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.drawPolygon(chevron, std::size(chevron));
  painter.setPen(Qt::NoPen);
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
