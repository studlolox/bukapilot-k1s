#pragma once

#include <QMouseEvent>
#include <QRect>
#include <QStackedLayout>
#include <QWidget>

#include "selfdrive/ui/qt/widgets/cameraview.h"
#include "selfdrive/ui/ui.h"


// ***** onroad widgets *****

class OnroadHud : public QWidget {
  Q_OBJECT
  Q_PROPERTY(QString speed MEMBER speed NOTIFY valueChanged);
  Q_PROPERTY(QString speedUnit MEMBER speedUnit NOTIFY valueChanged);
  Q_PROPERTY(QString maxSpeed MEMBER maxSpeed NOTIFY valueChanged);
  Q_PROPERTY(QString temperature MEMBER temperature NOTIFY valueChanged);
  Q_PROPERTY(float steerAngleDeg MEMBER steerAngleDeg NOTIFY valueChanged);
  Q_PROPERTY(bool steerOverride MEMBER steerOverride NOTIFY valueChanged);
  Q_PROPERTY(bool lateralActive MEMBER lateralActive NOTIFY valueChanged);
  Q_PROPERTY(int thermalStatus MEMBER thermalStatus NOTIFY valueChanged);
  Q_PROPERTY(bool is_cruise_set MEMBER is_cruise_set NOTIFY valueChanged);
  Q_PROPERTY(bool engageable MEMBER engageable NOTIFY valueChanged);
  Q_PROPERTY(bool dmActive MEMBER dmActive NOTIFY valueChanged);
  Q_PROPERTY(bool hideDM MEMBER hideDM NOTIFY valueChanged);
  Q_PROPERTY(int status MEMBER status NOTIFY valueChanged);
  Q_PROPERTY(float steerTorque MEMBER steerTorque NOTIFY valueChanged);
  Q_PROPERTY(bool steerSaturated MEMBER steerSaturated NOTIFY valueChanged);
  Q_PROPERTY(int laneChangeDirection MEMBER laneChangeDirection NOTIFY valueChanged);
  Q_PROPERTY(bool wheelCritical MEMBER wheelCritical NOTIFY valueChanged);

public:
  explicit OnroadHud(QWidget *parent);
  void updateState(const UIState &s);
  void manualMouseEvent(QMouseEvent *e);

private:
  void drawCapsule(QPainter &p, const QRect &rc);
  void drawSetSpeedBox(QPainter &p, const QRect &rc);
  void drawCurrentSpeed(QPainter &p, int cx, int y);
  void drawMiciSteeringWheel(QPainter &p, int cx, int cy, float angle, bool critical);
  void drawTorqueArcBar(QPainter &p, int cx, int cy, int arc_radius, float torque, bool saturated);
  void drawTurnIntent(QPainter &p, int cx, int cy, int dir);
  void drawStatusCapsule(QPainter &p, const QRect &rc, const QString &label, const QString &val, const QColor &color);
  void drawActionBtn(QPainter &p, int x, int y, QPixmap &img, bool active);
  void drawIcon(QPainter &p, int x, int y, QPixmap &img, QBrush bg, float opacity);
  void drawText(QPainter &p, int x, int y, const QString &text, int alpha = 255);
  void paintEvent(QPaintEvent *event) override;

  QPixmap engage_img;
  QPixmap dm_img;
  QPixmap settings_img;
  QPixmap wheel_img;
  QPixmap wheel_critical_img;
  QPixmap turn_intent_img;
  QPixmap exclamation_img;
  const int radius = 192;
  const int img_size = (radius / 2) * 1.5;
  QString speed;
  QString speedUnit;
  QString maxSpeed;
  QString temperature;
  float steerAngleDeg = 0.0f;
  bool steerOverride = false;
  bool lateralActive = false;
  float steerTorque = 0.0f;
  bool steerSaturated = false;
  int laneChangeDirection = 0;
  bool wheelCritical = false;
  int thermalStatus = 0;
  bool is_cruise_set = false;
  bool engageable = false;
  bool dmActive = false;
  bool hideDM = false;
  int status = STATUS_DISENGAGED;

signals:
  void valueChanged();
  void openSettings();
};

class OnroadAlerts : public QWidget {
  Q_OBJECT

public:
  OnroadAlerts(QWidget *parent = 0) : QWidget(parent) {};
  void updateAlert(const Alert &a, const QColor &color);

protected:
  void paintEvent(QPaintEvent*) override;

private:
  QColor bg;
  Alert alert = {};
};

// container window for the NVG UI
class NvgWindow : public CameraViewWidget {
  Q_OBJECT

public:
  explicit NvgWindow(VisionStreamType type, QWidget* parent = 0) : CameraViewWidget("camerad", type, true, parent) {}

protected:
  void paintGL() override;
  void initializeGL() override;
  void showEvent(QShowEvent *event) override;
  void updateFrameMat(int w, int h) override;
  void drawLaneLines(QPainter &painter, const UIScene &scene);
  void drawLead(QPainter &painter, const cereal::ModelDataV2::LeadDataV3::Reader &lead_data, const QPointF &vd);
  inline QColor redColor(int alpha = 255) { return QColor(201, 34, 49, alpha); }
  double prev_draw_t = 0;
};

// container for all onroad widgets
class OnroadWindow : public QWidget {
  Q_OBJECT

public:
  OnroadWindow(QWidget* parent = 0);
  bool isMapVisible() const { return map && map->isVisible(); }

private:
  void paintEvent(QPaintEvent *event);
  void mousePressEvent(QMouseEvent* e) override;
  OnroadHud *hud;
  OnroadAlerts *alerts;
  NvgWindow *nvg;
  QColor bg = bg_colors[STATUS_DISENGAGED];
  QWidget *map = nullptr;
  QHBoxLayout* split;

private slots:
  void offroadTransition(bool offroad);
  void updateState(const UIState &s);

signals:
  void openSettings();
};
