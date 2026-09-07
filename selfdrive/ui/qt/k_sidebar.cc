#include "selfdrive/ui/qt/k_sidebar.h"

#include <QVBoxLayout>
#include <QWidget>

#include "selfdrive/ui/qt/util.h"

Sidebar::Sidebar(QWidget *parent) : QFrame(parent) {
  auto layout = new QVBoxLayout(this);
  layout->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  layout->setContentsMargins(15, 30, 15, 30);
  layout->setSpacing(20);

  auto logo = new QLabel();
  logo->setPixmap(loadPixmap("../assets/kommu/logo.png", {170, 170}));
  logo->setAlignment(Qt::AlignHCenter);
  logo->setStyleSheet("border: none; background: transparent; margin-bottom: 15px;");
  layout->addWidget(logo);

  alert_empty_icon = loadPixmap("../assets/kommu/alerts.png", {56, 56});
  alert_unread_icon = loadPixmap("../assets/kommu/alerts_red.png", {56, 56});
  auto alerts = new SidebarItem("Alerts", "../assets/kommu/alerts.png", this);
  alert_icon = alerts->icon;
  connect(alerts, &SidebarItem::clicked, this, &Sidebar::openAlerts);
  layout->addWidget(alerts);

  auto settings = new SidebarItem("Settings", "../assets/kommu/settings.png", this);
  connect(settings, &SidebarItem::clicked, this, &Sidebar::openSettings);
  layout->addWidget(settings);

  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  setFixedWidth(250);
  setStyleSheet(R"(
    Sidebar {
      background-color: #0D0F15;
      border-right: 1.5px solid rgba(255, 255, 255, 0.08);
    }
  )");
}

void Sidebar::setAlertCount(int count) {
  alert_icon->setPixmap((count > 0) ? alert_unread_icon : alert_empty_icon);
}

SidebarItem::SidebarItem(const QString& label, const QString &iconPath, QWidget *parent) : ClickableWidget(parent) {
  auto layout = new QVBoxLayout(this);
  layout->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  layout->setContentsMargins(12, 16, 12, 16);
  layout->setSpacing(8);

  icon = new QLabel();
  icon->setPixmap(loadPixmap(iconPath, {56, 56}));
  icon->setAlignment(Qt::AlignHCenter);
  icon->setStyleSheet("border: none; background: transparent;");
  layout->addWidget(icon);

  text = new QLabel(label);
  text->setAlignment(Qt::AlignHCenter);
  text->setStyleSheet("font-size: 24px; font-weight: 600; color: #E2E8F0; border: none; background: transparent;");
  layout->addWidget(text);

  setStyleSheet(R"(
    SidebarItem {
      background-color: rgba(255, 255, 255, 0.04);
      border: 1px solid rgba(255, 255, 255, 0.06);
      border-radius: 18px;
    }
    SidebarItem:hover {
      background-color: rgba(255, 255, 255, 0.08);
      border: 1px solid rgba(255, 255, 255, 0.12);
    }
    SidebarItem:pressed {
      background-color: rgba(255, 255, 255, 0.14);
    }
  )");
}

