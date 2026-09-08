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
  QVBoxLayout *main_layout = new QVBoxLayout(this);
  main_layout->setMargin(0);
  main_layout->setSpacing(0);
  QStackedLayout *stacked_layout = new QStackedLayout;
  stacked_layout->setStackingMode(QStackedLayout::StackAll);
  main_layout->addLayout(stacked_layout);

  QStackedLayout *road_view_layout = new QStackedLayout;
  road_view_layout->setStackingMode(QStackedLayout::StackAll);
  nvg = new NvgWindow(VISION_STREAM_RGB_BACK, this);
  road_view_layout->addWidget(nvg);
  hud = new OnroadHud(this);
  road_view_layout->addWidget(hud);

  QWidget *split_wrapper = new QWidget;
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
  QObject::connect(uiState(), &UIState::uiUpdate, this,
                   &OnroadWindow::updateState);
  QObject::connect(uiState(), &UIState::offroadTransition, this,
                   &OnroadWindow::offroadTransition);
  QObject::connect(hud, &OnroadHud::openSettings, this,
                   &OnroadWindow::openSettings);
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

void OnroadWindow::mousePressEvent(QMouseEvent *e) {
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
      MapWindow *m = new MapWindow(get_mapbox_settings());
      map = m;

      QObject::connect(uiState(), &UIState::offroadTransition, m,
                       &MapWindow::offroadTransition);

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
  nvg->setStreamType(wide_cam ? VISION_STREAM_RGB_WIDE
                              : VISION_STREAM_RGB_BACK);
}

void OnroadWindow::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.fillRect(rect(), QColor(13, 17, 23, 255));

  // Sleek luminous ambient engagement rim
  if (bg != bg_colors[STATUS_DISENGAGED]) {
    QPen glowPen(QColor(bg.red(), bg.green(), bg.blue(), 230), 4);
    p.setPen(glowPen);
    p.setBrush(Qt::NoBrush);
    p.drawRect(rect().adjusted(2, 2, -2, -2));
  }
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
    QLinearGradient accent_bar(card_rc.left(), card_rc.top(), card_rc.right(),
                               card_rc.top());
    accent_bar.setColorAt(0.0, QColor(bg.red(), bg.green(), bg.blue(), 0));
    accent_bar.setColorAt(0.5, QColor(bg.red(), bg.green(), bg.blue(), 220));
    accent_bar.setColorAt(1.0, QColor(bg.red(), bg.green(), bg.blue(), 0));
    p.setPen(Qt::NoPen);
    p.setBrush(accent_bar);
    p.drawRoundedRect(QRect(card_x + 36, card_y + 4, card_w - 72, 6), 3, 3);

    // Text 1 (Headline)
    configFont(p, "Inter", 66, "Bold");
    p.setPen(QColor(255, 255, 255));
    p.drawText(QRect(card_x + 40, card_y + 36, card_w - 80, 96),
               Qt::AlignHCenter | Qt::AlignTop, alert.text1);

    // Text 2 (Instruction / subtext)
    configFont(p, "Inter", 48, "Medium");
    p.setPen(QColor(209, 213, 219)); // Slate-300 silver
    p.drawText(QRect(card_x + 40, card_y + 144, card_w - 80, 110),
               Qt::AlignHCenter | Qt::TextWordWrap, alert.text2);

  } else if (alert.size == cereal::ControlsState::AlertSize::FULL) {
    // 3. FULL ALERT: Urgent full-screen takeover with dark radial vignette
    QRect full_rc = rect();

    QRadialGradient bg_vignette(full_rc.center().x(), full_rc.center().y(),
                                full_rc.width() * 0.75);
    bg_vignette.setColorAt(0.0, QColor(bg.red(), bg.green(), bg.blue(), 220));
    bg_vignette.setColorAt(
        1.0, QColor(bg.red() / 2, bg.green() / 2, bg.blue() / 2, 250));
    p.setPen(Qt::NoPen);
    p.setBrush(bg_vignette);
    p.drawRect(full_rc);

    bool long_text = alert.text1.length() > 15;
    configFont(p, "Inter", long_text ? 110 : 140, "Bold");
    p.setPen(QColor(255, 255, 255));
    p.drawText(
        QRect(60, full_rc.top() + (long_text ? 220 : 250), width() - 120, 500),
        Qt::AlignHCenter | Qt::TextWordWrap, alert.text1);

    configFont(p, "Inter", 72, "Medium");
    p.setPen(QColor(255, 255, 255, 230));
    p.drawText(QRect(60, full_rc.height() - (long_text ? 360 : 420),
                     width() - 120, 300),
               Qt::AlignHCenter | Qt::TextWordWrap, alert.text2);
  }
}

// OnroadHud
OnroadHud::OnroadHud(QWidget *parent) : QWidget(parent) {
  dm_img = loadPixmap("../assets/img_driver_face.png", {img_size, img_size});
  settings_img =
      loadPixmap("../assets/kommu/settings.png", {img_size, img_size});
  exp_img =
      loadPixmap("../assets/icons_mici/experimental.png", {img_size, img_size});
  chffr_wheel_img =
      loadPixmap("../assets/img_chffr_wheel.png", {img_size, img_size});
  wheel_img = loadPixmap("../assets/icons_mici/wheel.png", {img_size, img_size});
  wheel_critical_img =
      loadPixmap("../assets/icons_mici/wheel_critical.png", {img_size, img_size});
  turn_intent_img =
      loadPixmap("../assets/icons_mici/turn_intent_left.png", {38, 38});
  exclamation_img =
      loadPixmap("../assets/icons_mici/exclamation_point.png", {12, 44});

  connect(this, &OnroadHud::valueChanged, [=] { update(); });
}

void OnroadHud::updateState(const UIState &s) {
  const int SET_SPEED_NA = 255;
  const SubMaster &sm = *(s.sm);
  const auto cs = sm["controlsState"].getControlsState();

  float maxspeed =
      sm["carState"].getCarState().getCruiseState().getSpeedCluster() *
      MS_TO_KPH;
  bool cruise_set = maxspeed > 0 && (int)maxspeed != SET_SPEED_NA;
  if (cruise_set && !s.scene.is_metric) {
    maxspeed *= KM_TO_MILE;
  }
  QString maxspeed_str =
      cruise_set ? QString::number(std::nearbyint(maxspeed)) : "–";
  float cur_speed_hud = sm["carState"].getCarState().getVEgoCluster();
  float cur_speed = (cur_speed_hud == 0.0)
                        ? sm["carState"].getCarState().getVEgo()
                        : cur_speed_hud;
  cur_speed =
      std::max(0.0, cur_speed * (s.scene.is_metric ? MS_TO_KPH : MS_TO_MPH));
  float temp = sm["deviceState"].getDeviceState().getAmbientTempC() *
               (s.scene.is_metric ? 1 : 1.8);
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

  bool critical =
      (cs.getAlertStatus() == cereal::ControlsState::AlertStatus::CRITICAL);

  // Extract setDistance from cruiseState (1 = Aggressive, 2 = Normal, 3 =
  // Chill, 4 = Auto)
  int dist_bars = 3;
  auto set_dist =
      sm["carState"].getCarState().getCruiseState().getSetDistance();
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

  float a_ego = 0.0f;
  if (sm.alive("carState") && sm.valid("carState")) {
    a_ego = sm["carState"].getCarState().getAEgo();
  }
  if (getenv("FORCE_ONROAD") != NULL && a_ego == 0.0f) {
    a_ego = 0.25f;
  }

  setProperty("is_cruise_set", cruise_set);
  setProperty("speed", QString::number(std::nearbyint(cur_speed)));
  setProperty("maxSpeed", maxspeed_str);
  setProperty("speedUnit", s.scene.is_metric ? "km/h" : "mph");
  setProperty("temperature", QString::number(std::nearbyint(temp)) +
                                 (s.scene.is_metric ? "°C" : "°F"));
  setProperty("aEgo", a_ego);
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
  setProperty("hideDM",
              cs.getAlertSize() != cereal::ControlsState::AlertSize::NONE);
  setProperty("status", s.status);

  // update engageability and DM icons at 2Hz
  if (getenv("FORCE_ONROAD") != NULL) {
    setProperty("engageable", true);
    setProperty("dmActive", true);
  } else if (sm.frame % (UI_FREQ / 2) == 0) {
    setProperty("engageable", cs.getEngageable() || cs.getEnabled());
    setProperty("dmActive", sm["driverMonitoringState"]
                                .getDriverMonitoringState()
                                .getIsActiveMode());
  }
}

void OnroadHud::manualMouseEvent(QMouseEvent *e) {
  QPoint pt = mapFromGlobal(e->globalPos());

  int settings_cy = rect().bottom() - 125;
  int settings_cx = 140;
  int mode_cx = 380;
  int wheel_cx = rect().right() - 380;
  int wheel_cy = settings_cy;
  int dm_cx = rect().right() - 140;
  int dm_cy = settings_cy;

  // Settings button (bottom-left)
  if (std::hypot(pt.x() - settings_cx, pt.y() - settings_cy) <= 80 ||
      (e->pos().x() >= settings_cx - 80 && e->pos().x() <= settings_cx + 80 &&
       e->pos().y() >= settings_cy - 80 && e->pos().y() <= settings_cy + 80)) {
    emit openSettings();
    return;
  }

  // Mode button (pill button: E2E / LANES)
  QRect mode_rc(mode_cx - 120, settings_cy - 50, 240, 100);
  if (mode_rc.contains(pt) || mode_rc.contains(e->pos())) {
    bool next_e2e = !experimental_mode;
    Params().putBool("EndToEndToggle", next_e2e);
    setProperty("experimental_mode", next_e2e);
    return;
  }

  // Steering Wheel button (right inner)
  if (std::hypot(pt.x() - wheel_cx, pt.y() - wheel_cy) <= 80 ||
      (e->pos().x() >= wheel_cx - 80 && e->pos().x() <= wheel_cx + 80 &&
       e->pos().y() >= wheel_cy - 80 && e->pos().y() <= wheel_cy + 80)) {
    emit openSettings();
    return;
  }

  // Driver Monitoring disc (far right)
  if (std::hypot(pt.x() - dm_cx, pt.y() - dm_cy) <= 80) {
    emit openSettings();
    return;
  }

  // Tap MAX Speed capsule to cycle distance gap (1 -> 2 -> 3 -> 1)
  QRect max_rc(45, 35, 380, 140);
  if (max_rc.contains(pt) || max_rc.contains(e->pos())) {
    int next_bars = (distanceBars % 3) + 1;
    setProperty("distanceBars", next_bars);
    return;
  }
}

void OnroadHud::drawCapsule(QPainter &p, const QRect &rc) {
  p.save();
  p.setRenderHint(QPainter::Antialiasing);

  int r = rc.height() / 2;
  // Translucent obsidian frosted glass
  p.setPen(QPen(QColor(255, 255, 255, 50), 2.0));
  p.setBrush(QColor(18, 25, 36, 215));
  p.drawRoundedRect(rc, r, r);

  // Subtle specular highlight line along the top inner edge
  QLinearGradient highlight(rc.left() + 30, rc.top() + 2, rc.right() - 30, rc.top() + 2);
  highlight.setColorAt(0.0, QColor(255, 255, 255, 0));
  highlight.setColorAt(0.5, QColor(255, 255, 255, 85));
  highlight.setColorAt(1.0, QColor(255, 255, 255, 0));
  p.setPen(QPen(highlight, 1.5));
  p.drawLine(rc.left() + 35, rc.top() + 3, rc.right() - 35, rc.top() + 3);

  p.restore();
}

void OnroadHud::drawSetSpeedBox(QPainter &p, const QRect &rc) {
  drawCapsule(p, rc);

  QColor max_color = QColor(0, 245, 212); // Neon cyan by default when engaged
  if (is_cruise_set) {
    if (status == STATUS_ENGAGED || getenv("FORCE_ONROAD") != NULL) {
      max_color = QColor(0, 245, 212);
    } else if (status == STATUS_WARNING || steerOverride) {
      max_color = QColor(245, 158, 11);
    } else {
      max_color = QColor(145, 155, 149);
    }
  }

  // Left sub-badge: circular dark glass inset for MAX speed
  int badge_d = rc.height() - 16;
  QRect badge_rc(rc.left() + 8, rc.top() + 8, badge_d, badge_d);
  p.save();
  p.setPen(QPen(QColor(255, 255, 255, 35), 1.5));
  p.setBrush(QColor(10, 14, 20, 200));
  p.drawEllipse(badge_rc);

  // "MAX" label
  configFont(p, "Inter", 24, "Bold");
  p.setPen(QColor(156, 163, 175)); // Silver gray
  drawText(p, badge_rc.center().x(), rc.top() + 42, "MAX", 230);

  // Set speed number
  if (is_cruise_set) {
    configFont(p, "Inter", 56, "Bold");
    p.setPen(QColor(255, 255, 255));
    drawText(p, badge_rc.center().x(), rc.top() + 98, maxSpeed, 255);
  } else {
    configFont(p, "Inter", 56, "Bold");
    p.setPen(QColor(114, 114, 114));
    drawText(p, badge_rc.center().x(), rc.top() + 98, "–", 200);
  }
  p.restore();

  // Middle sub-block: 3 stacked horizontal glowing cyan distance bars
  const int bar_w = 46;
  const int bar_h = 8;
  const int bar_spacing = 8;
  int bars_start_y = rc.center().y() - (3 * bar_h + 2 * bar_spacing) / 2;
  p.save();
  p.setPen(Qt::NoPen);
  for (int i = 0; i < 3; ++i) {
    int by = bars_start_y + i * (bar_h + bar_spacing);
    QRect bar_rc(rc.left() + badge_d + 20, by, bar_w, bar_h);
    bool is_lit = (3 - i) <= distanceBars;
    p.setBrush(is_lit ? max_color : QColor(255, 255, 255, 35));
    p.drawRoundedRect(bar_rc, 4, 4);
    if (is_lit) {
      p.setPen(QPen(QColor(max_color.red(), max_color.green(), max_color.blue(), 80), 3));
      p.setBrush(Qt::NoBrush);
      p.drawRoundedRect(bar_rc, 4, 4);
      p.setPen(Qt::NoPen);
    }
  }
  p.restore();

  // Right sub-block: Lead vehicle distance in meters e.g. "38m"
  p.save();
  int text_left = rc.left() + badge_d + 20 + bar_w + 16;
  QRect text_rc(text_left, rc.top(), rc.right() - text_left - 10, rc.height());
  QString lead_str = "38m";
  if (hasLead && leadDistance > 0.5f) {
    lead_str = QString::number(std::round(leadDistance)) + "m";
  } else if (getenv("FORCE_ONROAD") != NULL) {
    lead_str = "38m";
  }
  configFont(p, "Inter", 50, "Bold");
  p.setPen(QColor(255, 255, 255));
  p.drawText(text_rc, Qt::AlignVCenter | Qt::AlignLeft, lead_str);
  p.restore();
}

void OnroadHud::drawDistanceBars(QPainter &p, int cx, int y, int bars,
                                 const QColor &active_color) {
  // Retained for compatibility
}

void OnroadHud::drawSpeedHaloArc(QPainter &p, int cx, int cy, float cur_spd, float max_spd, float a_ego) {
  p.save();
  p.setRenderHint(QPainter::Antialiasing);

  // Center the halo arc so it sweeps smoothly over the speedometer numbers
  const float R = 240.0f;
  const float arc_center_y = cy + 95.0f; // cy = 160 -> arc_center_y = 255
  QRectF arcRect(cx - R, arc_center_y - R, 2.0f * R, 2.0f * R);

  const float start_deg = 155.0f;
  const float end_deg = 25.0f;
  const float total_span = start_deg - end_deg; // 130 degrees

  // 1. Background Track
  int bgStart = (int)std::round(start_deg * 16.0f);
  int bgSpan = (int)std::round(-total_span * 16.0f);

  p.setBrush(Qt::NoBrush);
  p.setPen(QPen(QColor(255, 255, 255, 25), 14, Qt::SolidLine, Qt::RoundCap));
  p.drawArc(arcRect, bgStart, bgSpan);

  p.setPen(QPen(QColor(15, 20, 28, 200), 9, Qt::SolidLine, Qt::RoundCap));
  p.drawArc(arcRect, bgStart, bgSpan);

  // 2. Active arc sweep calculation
  float ref_max = (max_spd > 20.0f) ? std::max(max_spd, 120.0f) : 120.0f;
  float ratio = std::clamp(cur_spd / ref_max, 0.0f, 1.0f);
  float active_span = ratio * total_span;

  // 3. Dynamic color transitions
  QColor core_col, glow_col;
  if (status == STATUS_ENGAGED || getenv("FORCE_ONROAD") != NULL) {
    if (a_ego >= 0.05f) {
      core_col = QColor(13, 248, 122);
      glow_col = QColor(13, 248, 122, 110);
    } else if (a_ego >= -0.5f) {
      core_col = QColor(240, 244, 248);
      glow_col = QColor(240, 244, 248, 80);
    } else {
      core_col = QColor(245, 158, 11);
      glow_col = QColor(245, 158, 11, 120);
    }
  } else if (steerOverride) {
    core_col = QColor(255, 255, 255, 220);
    glow_col = QColor(255, 255, 255, 60);
  } else {
    core_col = QColor(140, 150, 160, 140);
    glow_col = QColor(140, 150, 160, 35);
  }

  if (active_span > 2.0f) {
    int activeStart = (int)std::round(start_deg * 16.0f);
    int activeSpanVal = (int)std::round(-active_span * 16.0f);

    // Soft luminous outer glow
    p.setPen(QPen(glow_col, 16, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(arcRect, activeStart, activeSpanVal);

    // Solid bright core arc
    p.setPen(QPen(core_col, 9, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(arcRect, activeStart, activeSpanVal);

    // Dynamic tip indicator pip
    float current_angle_rad = (start_deg - active_span) * (3.14159265f / 180.0f);
    float pip_x = cx + R * std::cos(current_angle_rad);
    float pip_y = arc_center_y - R * std::sin(current_angle_rad);

    p.setPen(QPen(QColor(255, 255, 255, 250), 2.5f));
    p.setBrush(core_col);
    p.drawEllipse(QPointF(pip_x, pip_y), 7.0f, 7.0f);
  }

  p.restore();
}

void OnroadHud::drawCurrentSpeed(QPainter &p, int cx, int y) {
  // 1. Draw Tachymetric Speed Halo Arc
  drawSpeedHaloArc(p, cx, y, speed.toFloat(), maxSpeed.toFloat(), aEgo);

  // 2. Monolithic High-Contrast Speed Numerals
  configFont(p, "Inter", 160, "Bold");
  p.setPen(QColor(255, 255, 255));
  drawText(p, cx, y, speed, 255);

  // 3. Speed Unit
  configFont(p, "Inter", 44, "Medium");
  p.setPen(QColor(209, 213, 219)); // Slate silver
  drawText(p, cx, y + 74, speedUnit, 220);
}

void OnroadHud::drawBottomTorqueArcBar(QPainter &p, int cx, int y, float torque,
                                       bool saturated, float max_half_w) {
  p.save();
  p.setRenderHint(QPainter::Antialiasing);

  // Prominent curved instrument reactor arc
  const float R = 270.0f;
  const float arc_center_y = y + 50.0f; // Center grounded slightly below bottom
  const float start_deg = 155.0f;
  const float end_deg = 25.0f;
  const float total_span = start_deg - end_deg; // 130 degrees
  QRectF arcRect(cx - R, arc_center_y - R, 2.0f * R, 2.0f * R);

  // 1. Background Track
  int bgStart = (int)std::round(start_deg * 16.0f);
  int bgSpan = (int)std::round(-total_span * 16.0f);

  // Outer ambient glow for track
  p.setBrush(Qt::NoBrush);
  p.setPen(QPen(QColor(0, 245, 212, 35), 24, Qt::SolidLine, Qt::RoundCap));
  p.drawArc(arcRect, bgStart, bgSpan);

  // Inner dark frosted obsidian track
  p.setPen(QPen(QColor(15, 20, 28, 230), 16, Qt::SolidLine, Qt::RoundCap));
  p.drawArc(arcRect, bgStart, bgSpan);

  // Inner subtle highlight edge
  p.setPen(QPen(QColor(255, 255, 255, 40), 2.0f, Qt::SolidLine, Qt::RoundCap));
  p.drawArc(arcRect, bgStart, bgSpan);

  // 2. Center Zero Reference Pip (illuminated vertical line at 12 o'clock)
  p.setPen(QPen(QColor(0, 245, 212, 255), 4.0f, Qt::SolidLine, Qt::RoundCap));
  p.drawLine(QPointF(cx, arc_center_y - R - 18), QPointF(cx, arc_center_y - R + 18));

  // 3. Torque Dynamics Calculation & Limit Warning Colors
  float clamped = std::clamp(torque, -1.0f, 1.0f);
  float abs_torque = std::abs(clamped);

  QColor arc_core_col = QColor(0, 245, 212, 255); // Neon cyan core
  QColor arc_glow_col = QColor(0, 245, 212, 110);

  if (saturated || abs_torque > 0.85f) {
    arc_core_col = QColor(248, 113, 113, 255); // Alert coral red
    arc_glow_col = QColor(248, 113, 113, 140);
  } else if (abs_torque > 0.60f) {
    arc_core_col = QColor(245, 158, 11, 255);  // Warning amber
    arc_glow_col = QColor(245, 158, 11, 120);
  } else if (lateralActive || getenv("FORCE_ONROAD") != NULL) {
    arc_core_col = QColor(0, 245, 212, 255);   // Luminous cyan / mint
    arc_glow_col = QColor(0, 245, 212, 110);
  } else {
    arc_core_col = QColor(140, 150, 160, 160);
    arc_glow_col = QColor(140, 150, 160, 40);
  }

  // 4. Growing Bilateral Steering Arc
  const float max_half_span = 55.0f; // degrees from 90° center
  float effective_torque = clamped;
  if (getenv("FORCE_ONROAD") != NULL && std::abs(effective_torque) < 0.05f) {
    effective_torque = -0.45f; // Active sweeping arc in demo
  }

  if (std::abs(effective_torque) > 0.02f) {
    float sweep_deg = -effective_torque * max_half_span;
    int activeStart = 90 * 16;
    int activeSpan = (int)std::round(sweep_deg * 16.0f);

    // Soft luminous outer glow
    p.setPen(QPen(arc_glow_col, 22, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(arcRect, activeStart, activeSpan);

    // Solid bright growing steering arc
    p.setPen(QPen(arc_core_col, 15, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(arcRect, activeStart, activeSpan);

    // Tip indicator pip at the edge of the sweep
    float tip_angle_rad = (90.0f + sweep_deg) * (3.14159265f / 180.0f);
    float tip_x = cx + R * std::cos(tip_angle_rad);
    float tip_y = arc_center_y - R * std::sin(tip_angle_rad);
    p.setPen(QPen(QColor(255, 255, 255, 240), 2.5f));
    p.setBrush(arc_core_col);
    p.drawEllipse(QPointF(tip_x, tip_y), 7.0f, 7.0f);
  }

  // 5. Digital Steering Angle Readout directly in the center
  QString angle_str = QString::asprintf("%+.1f°", steerAngleDeg);
  if (std::abs(steerAngleDeg) < 0.05f) {
    angle_str = "0.0°";
  }
  configFont(p, "Inter", 54, "Bold");
  QColor angle_col = QColor(255, 255, 255, 255);
  if (wheelCritical || saturated) {
    angle_col = QColor(248, 113, 113);
  } else if (steerOverride) {
    angle_col = QColor(245, 158, 11);
  } else if (lateralActive || getenv("FORCE_ONROAD") != NULL) {
    angle_col = QColor(255, 255, 255);
  }
  p.setPen(angle_col);
  drawText(p, cx, y - 120, angle_str, 255);
  p.restore();
}

void OnroadHud::drawSteerBtn(QPainter &p, int x, int y, float angle, bool critical, int dir) {
  p.save();
  p.setRenderHint(QPainter::Antialiasing);

  const int btn_r = radius / 2;
  QRect btn_rc(x - btn_r, y - btn_r, radius, radius);

  // 1. Disc background & dynamic border
  QColor border_col = QColor(255, 255, 255, 45);
  int border_w = 2;
  if (critical) {
    border_col = QColor(248, 113, 113, 230); // Alert coral red
    border_w = 4;
  } else if (steerOverride) {
    border_col = QColor(245, 158, 11, 220);  // Warning amber
    border_w = 3;
  } else if (lateralActive) {
    border_col = QColor(128, 216, 166, 220); // Emerald mint
    border_w = 3;
  }

  p.setBrush(QColor(15, 20, 28, 210));
  p.setPen(QPen(border_col, border_w));
  p.drawEllipse(btn_rc);

  // Outer active glow
  if (lateralActive) {
    p.setPen(QPen(QColor(border_col.red(), border_col.green(), border_col.blue(), 70), 6));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(btn_rc);
  }

  // 2. Rotating steering wheel inside disc
  p.save();
  p.translate(x, y);
  p.rotate(-angle);

  const QPixmap &wheel = critical ? wheel_critical_img : wheel_img;
  if (!wheel.isNull()) {
    p.setOpacity(lateralActive ? 1.0 : (status == STATUS_DISENGAGED ? 0.45 : 0.85));
    p.drawPixmap(-wheel.width() / 2, -wheel.height() / 2, wheel);
    p.setOpacity(1.0);
  }
  p.restore();

  // 3. Exclamation mark if critical
  if (critical && !exclamation_img.isNull()) {
    p.drawPixmap(x + 50, y - exclamation_img.height() / 2, exclamation_img);
  }

  // 4. Sequential Neon Turn Chevrons flanking the disc: « and »
  QColor turn_col_left = (dir == 1) ? QColor(0, 245, 212, 240) : QColor(0, 245, 212, 160);
  QColor turn_col_right = (dir == 2) ? QColor(0, 245, 212, 240) : QColor(0, 245, 212, 160);

  // Left Chevron «
  p.setPen(QPen(turn_col_left, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.drawLine(QPointF(x - btn_r - 20, y - 16), QPointF(x - btn_r - 34, y));
  p.drawLine(QPointF(x - btn_r - 34, y), QPointF(x - btn_r - 20, y + 16));

  // Right Chevron »
  p.setPen(QPen(turn_col_right, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.drawLine(QPointF(x + btn_r + 20, y - 16), QPointF(x + btn_r + 34, y));
  p.drawLine(QPointF(x + btn_r + 34, y), QPointF(x + btn_r + 20, y + 16));

  p.restore();
}

void OnroadHud::drawMiciSteeringWheel(QPainter &p, int cx, int cy, float angle, bool critical) {
  // Integrated into drawSteerBtn
}

void OnroadHud::drawTurnIntent(QPainter &p, int cx, int cy, int dir) {
  // Turn intent is integrated directly into drawSteerBtn
}

void OnroadHud::drawStatusCapsule(QPainter &p, const QRect &rc,
                                  const QString &label, const QString &val,
                                  const QColor &color) {
  drawCapsule(p, rc);

  configFont(p, "Inter", 54, "Bold");
  p.setPen(color);
  p.drawText(rc, Qt::AlignCenter, val);
}

void OnroadHud::drawActionBtn(QPainter &p, int x, int y, QPixmap &img,
                              bool active) {
  p.save();
  p.setRenderHint(QPainter::Antialiasing);

  const int btn_r = radius / 2;
  QRect btn_rc(x - btn_r, y - btn_r, radius, radius);

  // Frosted obsidian glass disc
  p.setPen(QPen(QColor(255, 255, 255, 45), 2.5));
  p.setBrush(QColor(15, 20, 28, active ? 210 : 140));
  p.drawEllipse(btn_rc);

  if (active) {
    p.setPen(QPen(QColor(0, 245, 212, 60), 6));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(btn_rc);
  }

  p.setOpacity(active ? 1.0 : 0.5);
  p.drawPixmap(x - img_size / 2, y - img_size / 2, img);
  p.setOpacity(1.0);

  p.restore();
}

void OnroadHud::drawModeBtn(QPainter &p, int x, int y, bool is_experimental) {
  p.save();
  p.setRenderHint(QPainter::Antialiasing);

  const int pill_w = 230;
  const int pill_h = 92;
  QRect pill_rc(x - pill_w / 2, y - pill_h / 2, pill_w, pill_h);
  int r = pill_h / 2;

  // Frosted obsidian pill background
  p.setBrush(QColor(15, 20, 28, 220));

  QColor rim_col = is_experimental ? QColor(245, 158, 11) : QColor(0, 245, 212);

  // Outer soft glow
  p.setPen(QPen(QColor(rim_col.red(), rim_col.green(), rim_col.blue(), 80), 8, Qt::SolidLine, Qt::RoundCap));
  p.drawRoundedRect(pill_rc, r, r);

  // Core neon rim
  p.setPen(QPen(QColor(rim_col.red(), rim_col.green(), rim_col.blue(), 230), 3.5, Qt::SolidLine, Qt::RoundCap));
  p.drawRoundedRect(pill_rc, r, r);

  // Text inside pill: "E2E / LANES"
  configFont(p, "Inter", 26, "Bold");
  p.setPen(rim_col);
  p.drawText(pill_rc, Qt::AlignCenter, is_experimental ? "E2E MODE" : "E2E / LANES");

  p.restore();
}

void OnroadHud::drawDriverMonitoringDisc(QPainter &p, int x, int y, bool active,
                                         float awareness, bool distracted,
                                         bool face_detected, float yaw,
                                         float pitch) {
  p.save();
  p.setRenderHint(QPainter::Antialiasing);

  const int btn_r = radius / 2;
  QRect btn_rc(x - btn_r, y - btn_r, radius, radius);

  // 1. Base dark frosted obsidian glass disc
  p.setBrush(QColor(15, 20, 28, active ? 210 : 140));
  p.setPen(QPen(QColor(255, 255, 255, 45), 2.0));
  p.drawEllipse(btn_rc);

  // 2. Awareness arc ring around the disc
  if (active) {
    const int ring_r = btn_r + 8;
    QRectF ring_rc(x - ring_r, y - ring_r, ring_r * 2, ring_r * 2);

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 25), 4, Qt::SolidLine, Qt::RoundCap));
    p.drawEllipse(ring_rc);

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

    // Outer subtle glow
    p.setPen(QPen(QColor(ring_col.red(), ring_col.green(), ring_col.blue(), 80), 10, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(ring_rc, start_angle, span_angle);

    // Solid arc
    p.setPen(QPen(ring_col, 5, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(ring_rc, start_angle, span_angle);
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
    p.drawEllipse(QPointF(gaze_x, gaze_y), 5.0f, 5.0f);
  }

  p.restore();
}

void OnroadHud::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  // 1. Subtle horizon top vignette
  QLinearGradient bg(0, 0, 0, 260);
  bg.setColorAt(0, QColor(0, 0, 0, 140));
  bg.setColorAt(0.7, QColor(0, 0, 0, 50));
  bg.setColorAt(1, QColor(0, 0, 0, 0));
  p.fillRect(0, 0, width(), 260, bg);

  // 2. MAX Speed & Distance Capsule (top-left) - 380x140
  QRect max_rc(45, 35, 380, 140);
  drawSetSpeedBox(p, max_rc);

  // 3. Current Speed (top-center)
  drawCurrentSpeed(p, rect().center().x(), 160);

  // 4. TEMP / Climate capsule (top-right) - 230x140
  int temp_w = 230;
  QRect temp_rc(rect().right() - temp_w - 45, 35, temp_w, 140);
  QColor temp_col = QColor(240, 243, 246);
  if (thermalStatus == (int)cereal::DeviceState::ThermalStatus::YELLOW) {
    temp_col = QColor(251, 191, 36);
  } else if (thermalStatus >= (int)cereal::DeviceState::ThermalStatus::RED) {
    temp_col = QColor(248, 113, 113);
  }
  drawStatusCapsule(p, temp_rc, "TEMP", temperature, temp_col);

  // 5. Bottom Controls & Telemetry Dock
  int settings_cy = rect().bottom() - 125;
  int settings_cx = 140;
  int mode_cx = 380;
  int wheel_cx = rect().right() - 380;
  int dm_cx = rect().right() - 140;

  // Center Steering Torque Reactor Arc
  int arc_cx = rect().center().x();
  int arc_y = rect().bottom();
  float available_half_w = 210.0f;
  drawBottomTorqueArcBar(p, arc_cx, arc_y, steerTorque, steerSaturated, available_half_w);

  // Outer Action Discs & Pills
  drawActionBtn(p, settings_cx, settings_cy, settings_img, engageable);
  drawModeBtn(p, mode_cx, settings_cy, experimental_mode);
  drawSteerBtn(p, wheel_cx, settings_cy, steerAngleDeg, wheelCritical, laneChangeDirection);

  if (!hideDM) {
    drawDriverMonitoringDisc(p, dm_cx, settings_cy, dmActive, dmAwareness,
                             dmDistracted, dmFaceDetected, dmYaw, dmPitch);
  }

  // Right-edge AI Confidence Ball
  int ball_x = rect().right() - 16;
  int ball_top_y = 120;
  int ball_bot_y = rect().bottom() - 160;
  drawConfidenceBall(p, ball_x, ball_top_y, ball_bot_y, modelConfidence);
}

void OnroadHud::drawConfidenceBall(QPainter &p, int x, int top_y, int bottom_y,
                                   float confidence) {
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
    glow.setColorAt(
        0.0, QColor(top_col.red(), top_col.green(), top_col.blue(), 110));
    glow.setColorAt(1.0,
                    QColor(top_col.red(), top_col.green(), top_col.blue(), 0));
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

void OnroadHud::drawText(QPainter &p, int x, int y, const QString &text,
                         int alpha) {
  QFontMetrics fm(p.font());
  QRect init_rect = fm.boundingRect(text);
  QRect real_rect = fm.boundingRect(init_rect, 0, text);
  real_rect.moveCenter({x, y - real_rect.height() / 2});

  p.setPen(QColor(0xff, 0xff, 0xff, alpha));
  p.drawText(real_rect.x(), real_rect.bottom(), text);
}

void OnroadHud::drawIcon(QPainter &p, int x, int y, QPixmap &img, QBrush bg,
                         float opacity) {
  p.setPen(Qt::NoPen);
  p.setBrush(bg);
  p.drawEllipse(x - radius / 2, y - radius / 2, radius, radius);
  p.setOpacity(opacity);
  p.drawPixmap(x - img_size / 2, y - img_size / 2, img);
}

// NvgWindow
void NvgWindow::initializeGL() {
  CameraViewWidget::initializeGL();
  qInfo() << "OpenGL version:"
          << QString((const char *)glGetString(GL_VERSION));
  qInfo() << "OpenGL vendor:" << QString((const char *)glGetString(GL_VENDOR));
  qInfo() << "OpenGL renderer:"
          << QString((const char *)glGetString(GL_RENDERER));
  qInfo() << "OpenGL language version:"
          << QString((const char *)glGetString(GL_SHADING_LANGUAGE_VERSION));

  prev_draw_t = millis_since_boot();
  setBackgroundColor(bg_colors[STATUS_DISENGAGED]);
}

void NvgWindow::updateFrameMat(int w, int h) {
  CameraViewWidget::updateFrameMat(w, h);

  UIState *s = uiState();
  s->fb_w = w;
  s->fb_h = h;
  auto intrinsic_matrix =
      s->wide_camera ? ecam_intrinsic_matrix : fcam_intrinsic_matrix;
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

QLinearGradient NvgWindow::getPathGradient(const UIScene &scene, float a_ego,
                                           bool engaged) {
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
      painter.setBrush(
          QColor::fromRgbF(1.0, 1.0, 1.0, scene.lane_line_probs[i]));
      painter.drawPolygon(scene.lane_line_vertices[i].v,
                          scene.lane_line_vertices[i].cnt);
    }
    // road edges
    for (int i = 0; i < std::size(scene.road_edge_vertices); ++i) {
      painter.setBrush(QColor::fromRgbF(
          1.0, 0, 0,
          std::clamp<float>(1.0 - scene.road_edge_stds[i], 0.0, 1.0)));
      painter.drawPolygon(scene.road_edge_vertices[i].v,
                          scene.road_edge_vertices[i].cnt);
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
  painter.setBrush(getPathGradient(
      scene, a_ego, s->engaged() || getenv("FORCE_ONROAD") != NULL));
  painter.drawPolygon(scene.track_vertices.v, scene.track_vertices.cnt);

  // Kinetic momentum chevrons along path centerline
  int v_cnt = scene.track_vertices.cnt;
  if (v_cnt >= 8 && (s->engaged() || getenv("FORCE_ONROAD") != NULL)) {
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);

    float v_ego = 0.0f;
    if (s->sm->alive("carState") && s->sm->valid("carState")) {
      v_ego = std::max(0.0f, (*s->sm)["carState"].getCarState().getVEgo());
    }
    if (getenv("FORCE_ONROAD") != NULL && v_ego == 0.0f) {
      v_ego = 22.0f;
    }

    double t = millis_since_boot() * 0.001;
    float phase = std::fmod(t * (0.8f + v_ego * 0.04f), 1.0f);

    int half_cnt = v_cnt / 2;
    for (int k = 0; k < 3; ++k) {
      float frac = std::fmod(phase + k * 0.333f, 1.0f);
      int idx = std::clamp((int)(frac * (half_cnt - 2)), 1, half_cnt - 2);

      QPointF p_left = scene.track_vertices.v[idx];
      QPointF p_right = scene.track_vertices.v[v_cnt - 1 - idx];
      QPointF center = (p_left + p_right) * 0.5f;

      float chevron_w = std::hypot(p_right.x() - p_left.x(), p_right.y() - p_left.y()) * 0.28f;
      float chevron_h = chevron_w * 0.45f;

      QPointF arrow[] = {
        QPointF(center.x(), center.y() - chevron_h),
        QPointF(center.x() + chevron_w, center.y() + chevron_h * 0.4f),
        QPointF(center.x(), center.y()),
        QPointF(center.x() - chevron_w, center.y() + chevron_h * 0.4f)
      };

      int chevron_alpha = (int)(180.0f * (1.0f - frac));
      QColor chevron_col(0, 245, 212, std::clamp(chevron_alpha, 0, 200));
      painter.setPen(Qt::NoPen);
      painter.setBrush(chevron_col);
      painter.drawPolygon(arrow, 4);
    }
    painter.restore();
  }
}

void NvgWindow::drawLead(
    QPainter &painter, const cereal::ModelDataV2::LeadDataV3::Reader &lead_data,
    const QPointF &vd) {
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
  QColor aura_color = (v_rel < -1.5f || d_rel < 15.0f) ? QColor(248, 113, 113)
                                                       : QColor(251, 191, 36);
  rad_glow.setColorAt(0.0, QColor(aura_color.red(), aura_color.green(),
                                  aura_color.blue(), (int)(fillAlpha * 0.75f)));
  rad_glow.setColorAt(
      1.0, QColor(aura_color.red(), aura_color.green(), aura_color.blue(), 0));
  painter.setBrush(rad_glow);
  painter.drawEllipse(QPointF(x, y + sz * 0.5f), glow_r, glow_r * 0.65f);

  // 2. Modern Aerodynamic Chevron Radar Target
  QPointF chevron[] = {{x, y},
                       {x + sz * 1.35f, y + sz},
                       {x, y + sz * 0.45f},
                       {x - sz * 1.35f, y + sz}};

  QColor chevron_fill = (fillAlpha > 120)
                            ? redColor(fillAlpha)
                            : QColor(255, 255, 255, (int)(fillAlpha * 0.85f));
  painter.setBrush(chevron_fill);
  painter.setPen(QPen(QColor(255, 255, 255, 230), 2.5, Qt::SolidLine,
                      Qt::RoundCap, Qt::RoundJoin));
  painter.drawPolygon(chevron, std::size(chevron));

  // 3. Fighter-Jet Corner Brackets & Ground Anchor Reticle
  float bw = sz * 1.35f;
  float bh = sz * 1.1f;
  float corner_len = sz * 0.45f;
  QPen bracket_pen(QColor(aura_color.red(), aura_color.green(), aura_color.blue(), std::max(160, (int)fillAlpha)), 2.5, Qt::SolidLine, Qt::RoundCap);
  painter.setPen(bracket_pen);

  // Top-left corner [
  painter.drawLine(QPointF(x - bw, y - bh * 0.2f + corner_len), QPointF(x - bw, y - bh * 0.2f));
  painter.drawLine(QPointF(x - bw, y - bh * 0.2f), QPointF(x - bw + corner_len, y - bh * 0.2f));

  // Top-right corner ]
  painter.drawLine(QPointF(x + bw, y - bh * 0.2f + corner_len), QPointF(x + bw, y - bh * 0.2f));
  painter.drawLine(QPointF(x + bw, y - bh * 0.2f), QPointF(x + bw - corner_len, y - bh * 0.2f));

  // Ground anchor shadow bracket beneath tires
  float ground_y = y + sz * 1.15f;
  painter.drawLine(QPointF(x - bw * 0.85f, ground_y), QPointF(x + bw * 0.85f, ground_y));

  // 4. Floating Distance Badge Tag (e.g. "38m")
  if (d_rel > 0.5f) {
    QString dist_text = QString::number(std::round(d_rel)) + "m";
    configFont(painter, "Inter", std::clamp((int)(sz * 0.75f), 18, 28), "Bold");
    QFontMetrics fm(painter.font());
    QRect text_rc = fm.boundingRect(dist_text);
    int tag_w = text_rc.width() + 16;
    int tag_h = text_rc.height() + 8;
    QRect badge_rc(x - tag_w / 2, y - bh * 0.2f - tag_h - 6, tag_w, tag_h);

    painter.setPen(QPen(QColor(255, 255, 255, 60), 1.0));
    painter.setBrush(QColor(15, 20, 28, 200));
    painter.drawRoundedRect(badge_rc, 6, 6);

    painter.setPen(QColor(255, 255, 255, 240));
    painter.drawText(badge_rc, Qt::AlignCenter, dist_text);
  }

  painter.setPen(Qt::NoPen);
}

void NvgWindow::paintGL() {
  UIState *s = uiState();
  if (!s->awake) {
    return;
  }

  // Thermal optimization for K1S:
  // When SoC temperature reaches Yellow (80°C+) or higher, decimate heavy rendering
  // to 10 FPS (every 2nd frame) to cut GPU/CPU thermal load and prevent reaching Red (90°C).
  static uint64_t paint_frame_count = 0;
  if (s->sm->allAliveAndValid({"deviceState"})) {
    auto thermal_status = (*s->sm)["deviceState"].getDeviceState().getThermalStatus();
    if (thermal_status >= cereal::DeviceState::ThermalStatus::YELLOW) {
      if ((++paint_frame_count % 2) != 0) {
        return;
      }
    }
  }

  CameraViewWidget::paintGL();

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
      if (leads[1].getProb() > .5 &&
          (std::abs(leads[1].getX()[0] - leads[0].getX()[0]) > 3.0)) {
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
