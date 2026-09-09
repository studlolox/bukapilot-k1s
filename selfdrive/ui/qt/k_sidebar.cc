#include "selfdrive/ui/qt/k_sidebar.h"

#include <QPainter>
#include <QVBoxLayout>
#include <QWidget>

#include "selfdrive/ui/qt/util.h"

// EzpilotLogoWidget: Pixel-faithful EZPilot geometric ribbon emblem + typography from mockup
class EzpilotLogoWidget : public QWidget {
  QPixmap logo_pixmap;

public:
  explicit EzpilotLogoWidget(QWidget *parent = nullptr) : QWidget(parent) {
    setFixedHeight(150);
    setFixedWidth(180);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    logo_pixmap = loadPixmap("../assets/images/ezpilot_logo.png");
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

    int cx = width() / 2;
    int cy = 48;

    // Soft ambient neon cyan halo behind the emblem
    QRadialGradient glow(cx, cy, 55);
    glow.setColorAt(0.0, QColor(0, 245, 212, 55));
    glow.setColorAt(0.6, QColor(0, 245, 212, 18));
    glow.setColorAt(1.0, QColor(0, 245, 212, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(glow);
    p.drawEllipse(QPointF(cx, cy), 55, 55);

    if (!logo_pixmap.isNull()) {
      int px = (width() - logo_pixmap.width()) / 2;
      int py = 6;
      p.drawPixmap(px, py, logo_pixmap);
    } else {
      // Vector fallback if image missing
      configFont(p, "Inter", 26, "Bold");
      p.setPen(QColor(0, 245, 212));
      p.drawText(rect(), Qt::AlignCenter, "ezpilot");
    }
  }
};

// SidebarItem: Frosted obsidian glass circular/squircle disc containing centered icon
SidebarItem::SidebarItem(const QString& label, const QString &iconPath, QWidget *parent)
    : ClickableWidget(parent) {
  setFixedSize(108, 108);
  setToolTip(label);

  auto layout = new QVBoxLayout(this);
  layout->setAlignment(Qt::AlignCenter);
  layout->setContentsMargins(0, 0, 0, 0);

  icon = new QLabel(this);
  icon->setPixmap(loadPixmap(iconPath, {60, 60}));
  icon->setAlignment(Qt::AlignCenter);
  icon->setStyleSheet("border: none; background: transparent;");
  layout->addWidget(icon);

  badge = new QLabel(this);
  badge->setAlignment(Qt::AlignCenter);
  badge->setFixedSize(28, 28);
  badge->move(76, 6);
  badge->setStyleSheet(R"(
    background-color: #EF4444;
    color: white;
    border: 2px solid #0D0F15;
    border-radius: 14px;
    font-size: 15px;
    font-weight: 700;
  )");
  badge->hide();

  setStyleSheet(R"(
    SidebarItem {
      background-color: rgba(255, 255, 255, 0.05);
      border: 1.5px solid rgba(255, 255, 255, 0.12);
      border-radius: 34px;
    }
    SidebarItem:hover {
      background-color: rgba(0, 245, 212, 0.12);
      border: 1.5px solid rgba(0, 245, 212, 0.6);
    }
    SidebarItem:pressed {
      background-color: rgba(0, 245, 212, 0.22);
      border: 1.5px solid #00F5D4;
    }
  )");
}

void SidebarItem::setBadgeCount(int count) {
  if (count > 0) {
    badge->setText(QString::number(std::min(count, 99)));
    badge->show();
  } else {
    badge->hide();
  }
}

void SidebarItem::setIcon(const QString &iconPath) {
  icon->setPixmap(loadPixmap(iconPath, {60, 60}));
}

Sidebar::Sidebar(QWidget *parent) : QFrame(parent) {
  auto layout = new QVBoxLayout(this);
  layout->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
  layout->setContentsMargins(16, 40, 16, 36);
  layout->setSpacing(26);

  // 1. EZPilot Geometric Logo
  auto logo = new EzpilotLogoWidget(this);
  layout->addWidget(logo, 0, Qt::AlignHCenter);

  layout->addSpacing(10);

  // 2. Settings button
  settings_item = new SidebarItem("Settings", "../assets/kommu/settings.png", this);
  connect(settings_item, &SidebarItem::clicked, [=]() { emit openSettings(0); });
  layout->addWidget(settings_item, 0, Qt::AlignHCenter);

  // 3. Alerts button with unread count badge
  alerts_item = new SidebarItem("Alerts", "../assets/kommu/alerts.png", this);
  connect(alerts_item, &SidebarItem::clicked, this, &Sidebar::openAlerts);
  layout->addWidget(alerts_item, 0, Qt::AlignHCenter);

  // 4. Wi-Fi / Network button
  network_item = new SidebarItem("Wi-Fi", "../assets/kommu/network.png", this);
  connect(network_item, &SidebarItem::clicked, [=]() { emit openSettings(1); });
  layout->addWidget(network_item, 0, Qt::AlignHCenter);

  layout->addStretch();

  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  setFixedWidth(200);
  setStyleSheet(R"(
    Sidebar {
      background-color: #0C1017;
      border-right: 1.5px solid rgba(255, 255, 255, 0.08);
    }
  )");
}

void Sidebar::setAlertCount(int count) {
  alerts_item->setBadgeCount(count);
}

