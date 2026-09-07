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
  QString maxspeed_str = cruise_set ? QString::number(std::nearbyint(maxspeed)) : "N/A";
  float cur_speed_hud = sm["carState"].getCarState().getVEgoCluster();
  float cur_speed =  (cur_speed_hud == 0.0) ? sm["carState"].getCarState().getVEgo() : cur_speed_hud;
  cur_speed = std::max(0.0, cur_speed * (s.scene.is_metric ? MS_TO_KPH : MS_TO_MPH));
  float temp = sm["deviceState"].getDeviceState().getAmbientTempC() * (s.scene.is_metric ? 1 : 1.8);
  temp += s.scene.is_metric ? 0 : 32;

  float steer_angle = sm["carState"].getCarState().getSteeringAngleDeg();
  bool steer_override = sm["carState"].getCarState().getSteeringPressed();
  bool lat_active = cs.getEnabled();
  int thermal_st = (int)sm["deviceState"].getDeviceState().getThermalStatus();

  if (getenv("FORCE_ONROAD") != NULL && cur_speed == 0.0) {
    cur_speed = 78.0;
    maxspeed_str = "80";
    cruise_set = true;
    temp = 28.0;
    steer_angle = -4.5f;
    steer_override = false;
    lat_active = true;
    thermal_st = 0;
  }

  setProperty("is_cruise_set", cruise_set);
  setProperty("speed", QString::number(std::nearbyint(cur_speed)));
  setProperty("maxSpeed", maxspeed_str);
  setProperty("speedUnit", s.scene.is_metric ? "km/h" : "mph");
  setProperty("temperature", QString::number(std::nearbyint(temp)) + (s.scene.is_metric ? "°C" : "°F"));
  setProperty("steerAngleDeg", steer_angle);
  setProperty("steerOverride", steer_override);
  setProperty("lateralActive", lat_active);
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
  if (e->globalX() > x && e->globalY() > y && e->globalX() <= x + w && e->globalY() <= y + w)
      emit openSettings();
}

void OnroadHud::drawCapsule(QPainter &p, const QRect &rc) {
  p.setPen(QPen(QColor(255, 255, 255, 45), 2));
  p.setBrush(QColor(18, 22, 30, 180));
  p.drawRoundedRect(rc, 24, 24);
}

void OnroadHud::drawSteerWheel(QPainter &p, int cx, int cy, float angle, QColor color) {
  p.save();
  p.translate(cx, cy);
  p.rotate(angle);

  // Outer steering ring
  p.setPen(QPen(color, 3, Qt::SolidLine, Qt::RoundCap));
  p.setBrush(Qt::NoBrush);
  p.drawEllipse(-18, -18, 36, 36);

  // Center hub
  p.setBrush(color);
  p.setPen(Qt::NoPen);
  p.drawEllipse(-4, -4, 8, 8);

  // Spokes (left, right, bottom)
  p.setPen(QPen(color, 2.5, Qt::SolidLine, Qt::RoundCap));
  p.drawLine(-17, 0, -4, 0);
  p.drawLine(4, 0, 17, 0);
  p.drawLine(0, 4, 0, 17);

  p.restore();
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

  // Smooth cinematic header gradient
  QLinearGradient bg(0, 0, 0, header_h);
  bg.setColorAt(0, QColor(0, 0, 0, 160));
  bg.setColorAt(0.65, QColor(0, 0, 0, 60));
  bg.setColorAt(1, QColor(0, 0, 0, 0));
  p.fillRect(0, 0, width(), header_h, bg);

  // 1. MAX Speed capsule (top-left)
  QRect rc(bdr_s * 2, bdr_s * 1.5, 184, 202);
  drawCapsule(p, rc);

  configFont(p, "Inter", 36, "Medium");
  drawText(p, rc.center().x(), 95, "MAX", 220);
  if (is_cruise_set) {
    configFont(p, "Inter", 82, "Bold");
    drawText(p, rc.center().x(), 182, maxSpeed, 255);
  } else {
    configFont(p, "Inter", 74, "SemiBold");
    drawText(p, rc.center().x(), 182, maxSpeed, 100);
  }

  // 2. Real-time STEER Gauge capsule (next to MAX speed)
  QRect steer_rc(bdr_s * 2 + 184 + 20, bdr_s * 1.5, 184, 202);
  drawCapsule(p, steer_rc);

  configFont(p, "Inter", 36, "Medium");
  drawText(p, steer_rc.center().x(), 95, "STEER", 220);

  QColor steer_col = QColor(226, 232, 240);
  if (steerOverride) {
    steer_col = QColor(245, 158, 11);       // Amber on override
  } else if (lateralActive) {
    steer_col = QColor(16, 185, 129);       // Emerald when auto-steering
  }
  drawSteerWheel(p, steer_rc.center().x(), 136, steerAngleDeg, steer_col);

  QString angle_str = (steerAngleDeg > 0.0f ? "+" : "") + QString::number(std::round(steerAngleDeg)) + "°";
  configFont(p, "Inter", 42, "SemiBold");
  p.setPen(steer_col);
  drawText(p, steer_rc.center().x(), 205, angle_str, 255);

  // 3. Current Speed (center)
  configFont(p, "Inter", 180, "Bold");
  drawText(p, rect().center().x(), 200, speed);
  configFont(p, "Inter", 44, "Medium");
  drawText(p, rect().center().x(), 265, speedUnit, 200);

  // 4. TEMP capsule (top-right)
  QRect temp_rc(rect().right() - bdr_s * 2 - 184, bdr_s * 1.5, 184, 202);
  drawCapsule(p, temp_rc);

  configFont(p, "Inter", 36, "Medium");
  drawText(p, temp_rc.center().x(), 95, "TEMP", 220);

  QColor temp_col = QColor(240, 243, 246);
  if (thermalStatus == (int)cereal::DeviceState::ThermalStatus::YELLOW) {
    temp_col = QColor(251, 191, 36);        // Amber warning
  } else if (thermalStatus >= (int)cereal::DeviceState::ThermalStatus::RED) {
    temp_col = QColor(248, 113, 113);       // Coral alert
  }
  configFont(p, "Inter", 64, "Bold");
  p.setPen(temp_col);
  drawText(p, temp_rc.center().x(), 182, temperature, 255);

  // 5. Bottom floating action discs (DM & Settings)
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
