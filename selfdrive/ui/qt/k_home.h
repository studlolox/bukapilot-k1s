#pragma once

#include <QFrame>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QStackedLayout>
#include <QTextOption>
#include <QTimer>
#include <QWidget>

#include <unordered_map>
#include <vector>

#include "selfdrive/common/params.h"
#include "selfdrive/ui/qt/offroad/driverview.h"
#include "selfdrive/ui/qt/onroad.h"
#include "selfdrive/ui/qt/k_sidebar.h"
#include "selfdrive/ui/qt/widgets/k_alerts.h"
#include "selfdrive/ui/ui.h"

class StatusLabel : public QWidget {

Q_OBJECT

public:
  QPixmap icon;
  QString text;

  StatusLabel(const QString& text, QPixmap icon, QWidget *parent);

private:
  QFont font;
  QTextOption opt = QTextOption(Qt::AlignCenter);

  void paintEvent(QPaintEvent *e) override;
};

class AlertsManager; // fwd: k_alerts.h

class QrWidget : public QWidget {

Q_OBJECT

public:
    QrWidget(const char *content, QWidget *parent);
    void paintEvent(QPaintEvent *e) override;
    void setContent(const char *content);

private:
    QPixmap img;
};

class VehicleVisualizerWidget;
class ThermalGaugeWidget;
class StorageProgressWidget;

class OffroadHome : public QFrame {
  Q_OBJECT

public:
  bool hasSevereAlerts = false;
  explicit OffroadHome(QWidget* parent = 0);

signals:
  void openSettings(int panel = 0);
  void openDriverView();

public slots:
  void updateState(const UIState& s);

private:
  Params params;
  QLabel *clock_label;
  QLabel *wifi_pill;
  QLabel *gps_pill;

  VehicleVisualizerWidget *vehicle_visualizer;
  QLabel *vehicle_title;
  QLabel *vehicle_sub;
  QLabel *system_status_pill;
  QLabel *panda_status_label;
  QLabel *calib_status_label;
  QLabel *gps_status_label;

  ThermalGaugeWidget *thermal_gauge;
  StorageProgressWidget *storage_progress;
  QLabel *device_info_pill;

  QLabel *version_label;
};

class HomeWindow : public QWidget {
  Q_OBJECT

public:
  explicit HomeWindow(QWidget* parent = 0);

signals:
  void openSettings(int panel = 0);
  void openTerms();
  void openTraining();
  void closeSettings();

public slots:
  void offroadTransition(bool offroad);
  void showDriverView(bool show);
  void showSidebar(bool show);

protected:
  void mousePressEvent(QMouseEvent* e) override;

private:
  void showEvent(QShowEvent *event) override;
  void hideEvent(QHideEvent *event) override;
  void refresh();

  AlertsManager alerts;
  Sidebar *sidebar;
  OffroadHome *home;
  OnroadWindow *onroad;
  DriverViewWindow *driver_view;
  QStackedLayout *slayout;
  QTimer *timer;
};
