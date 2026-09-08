#pragma once

#include <QFrame>
#include <QLabel>

#include "selfdrive/ui/qt/widgets/controls.h"
#include "selfdrive/ui/ui.h"

class SidebarItem : public ClickableWidget {
  Q_OBJECT

public:
  SidebarItem(const QString& label, const QString &iconPath, QWidget *parent);
  void setBadgeCount(int count);
  void setIcon(const QString &iconPath);

private:
  QLabel *icon;
  QLabel *badge;
};

class Sidebar : public QFrame {
  Q_OBJECT

public:
  explicit Sidebar(QWidget* parent = 0);
  void setAlertCount(int count);

signals:
  void openAlerts();
  void openSettings(int panel = 0);
  void openTerms();
  void openTraining();

private:
  SidebarItem *alerts_item;
  SidebarItem *settings_item;
  SidebarItem *network_item;
};
