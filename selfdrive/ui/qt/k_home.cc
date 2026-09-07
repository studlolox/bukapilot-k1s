#define K_IMPL
#include "selfdrive/ui/qt/k_home.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QVBoxLayout>

#include <QrCode.hpp>

#include "selfdrive/ui/qt/util.h"
#include "selfdrive/ui/qt/widgets/controls.h"
#include "selfdrive/ui/qt/widgets/popup.h"

// HomeWindow: the container for the offroad and onroad UIs

HomeWindow::HomeWindow(QWidget* parent) : QWidget(parent) {
  QHBoxLayout *main_layout = new QHBoxLayout(this);
  main_layout->setMargin(0);
  main_layout->setSpacing(0);

  sidebar = new Sidebar(this);
  main_layout->addWidget(sidebar);
  connect(sidebar, &Sidebar::openAlerts, this, [=] {
    (new Popup("Alerts", alerts.text, Popup::OK, this))->exec();
  });
  connect(sidebar, &Sidebar::openSettings, this, &HomeWindow::openSettings);
  connect(sidebar, &Sidebar::openTerms, this, &HomeWindow::openTerms);
  connect(sidebar, &Sidebar::openTraining, this, &HomeWindow::openTraining);

  slayout = new QStackedLayout();
  main_layout->addLayout(slayout);

  home = new OffroadHome();
  slayout->addWidget(home);

  onroad = new OnroadWindow(this);
  connect(onroad, &OnroadWindow::openSettings, this, &HomeWindow::openSettings);
  slayout->addWidget(onroad);

  driver_view = new DriverViewWindow(this);
  connect(driver_view, &DriverViewWindow::done, [=] {
    showDriverView(false);
  });
  slayout->addWidget(driver_view);
  setAttribute(Qt::WA_NoSystemBackground);
  connect(uiState(), &UIState::offroadTransition, this, &HomeWindow::offroadTransition);
  connect(uiState(), &UIState::uiUpdate, home, &OffroadHome::updateState);

  timer = new QTimer(this);
  timer->callOnTimeout(this, &HomeWindow::refresh);

  if (getenv("FORCE_ONROAD") != NULL) {
    offroadTransition(false);
  }
}

void HomeWindow::showEvent(QShowEvent *event) {
  refresh();
  timer->start(1 * 1000);
}

void HomeWindow::hideEvent(QHideEvent *event) {
  timer->stop();
}

void HomeWindow::refresh() {
  alerts.refresh();
  sidebar->setAlertCount(alerts.unread);
  home->hasSevereAlerts = alerts.hasSevere;
}

void HomeWindow::showSidebar(bool show) {
  sidebar->setVisible(show);
}

void HomeWindow::offroadTransition(bool offroad) {
  sidebar->setVisible(offroad);
  if (offroad) {
    slayout->setCurrentWidget(home);
  } else {
    slayout->setCurrentWidget(onroad);
  }
}

void HomeWindow::showDriverView(bool show) {
  if (show) {
    emit closeSettings();
    slayout->setCurrentWidget(driver_view);
  } else {
    slayout->setCurrentWidget(home);
  }
  sidebar->setVisible(show == false);
}

void HomeWindow::mousePressEvent(QMouseEvent* e) {
  // Handle sidebar collapsing
  if (onroad->isVisible() && (!sidebar->isVisible() || e->x() > sidebar->width())) {
    sidebar->setVisible(!sidebar->isVisible() && !onroad->isMapVisible());
  }
}

StatusLabel::StatusLabel(const QString& text, QPixmap icon, QWidget *parent = 0)
  : QWidget(parent), text(text), icon(icon) {
  font.setPixelSize(40);
  font.setWeight(QFont::DemiBold);
  opt.setWrapMode(QTextOption::WordWrap);
  setMinimumWidth(QFontMetrics(font).horizontalAdvance(text) + 60);
}

void StatusLabel::paintEvent(QPaintEvent *e) {
  QWidget::paintEvent(e);
  QPainter p(this);
  p.setFont(font);
  p.drawPixmap(width() * 0.11, 0, width() * 0.78, width() * 0.78, icon);
  p.drawText(QRect(width() * 0.26, width() * 0.2, width() * 0.5, width() * 0.38), text, opt);
}

QFrame *horizontal_rule(QWidget *parent) {
  auto line = new QFrame(parent);
  line->setFrameShape(QFrame::StyledPanel);
  line->setStyleSheet(R"(
    margin-top: 10px;
    margin-bottom: 10px;
    border-width: 1px;
    border-bottom-style: solid;
    border-color: rgba(255, 255, 255, 0.08);
  )");
  line->setFixedHeight(2);
  return line;
}

QrWidget::QrWidget(const char *content, QWidget *parent) : QWidget(parent) {
  setContent(content);
}

void QrWidget::paintEvent(QPaintEvent *e) {
  QPainter p(this);
  p.fillRect(rect(), Qt::white);
  p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, false);
  const int pad = 20;
  p.drawPixmap(pad, pad, width() - 2 * pad, height() - 2 * pad, img);
}

void QrWidget::setContent(const char *content) {
  using qrcodegen::QrCode;

  QrCode qr = QrCode::encodeText(content, QrCode::Ecc::LOW);
  qint32 sz = qr.getSize();
  QImage im(sz, sz, QImage::Format_RGB32);

  QRgb black = qRgb(0, 0, 0);
  QRgb white = qRgb(255, 255, 255);
  for (int y = 0; y < sz; y++) {
    for (int x = 0; x < sz; x++) {
      im.setPixel(x, y, qr.getModule(x, y) ? black : white);
    }
  }
  img = QPixmap::fromImage(im, Qt::MonoOnly);
}

// OffroadHome: Modernized 3-card vehicle dashboard

OffroadHome::OffroadHome(QWidget* parent) : QFrame(parent) {
  auto main_layout = new QHBoxLayout(this);
  main_layout->setContentsMargins(35, 35, 35, 35);
  main_layout->setSpacing(25);

  const QString card_style = R"(
    QFrame {
      background-color: #141722;
      border: 1.5px solid rgba(255, 255, 255, 0.09);
      border-radius: 24px;
    }
  )";

  // ===== CARD 1: Vehicle & ADAS State =====
  auto card_vehicle = new QFrame(this);
  card_vehicle->setStyleSheet(card_style);
  auto v_layout = new QVBoxLayout(card_vehicle);
  v_layout->setContentsMargins(35, 35, 35, 35);
  v_layout->setSpacing(14);

  auto v_tag = new QLabel("VEHICLE & ADAS", card_vehicle);
  v_tag->setStyleSheet("font-size: 24px; font-weight: 600; color: #8E929B; letter-spacing: 1px; border: none; background: transparent;");
  v_layout->addWidget(v_tag);

  vehicle_title = new QLabel("Toyota Corolla Cross", card_vehicle);
  vehicle_title->setStyleSheet("font-size: 38px; font-weight: 700; color: #FFFFFF; border: none; background: transparent;");
  vehicle_title->setWordWrap(true);
  v_layout->addWidget(vehicle_title);

  auto vehicle_sub = new QLabel("TSS 2.0 (No-DSU ADAS)", card_vehicle);
  vehicle_sub->setStyleSheet("font-size: 26px; font-weight: 500; color: #A0A5B5; border: none; background: transparent;");
  v_layout->addWidget(vehicle_sub);

  v_layout->addWidget(horizontal_rule(card_vehicle));

  // System Status Pill
  system_status_pill = new QLabel("SYSTEM READY", card_vehicle);
  system_status_pill->setAlignment(Qt::AlignCenter);
  system_status_pill->setFixedHeight(62);
  system_status_pill->setStyleSheet(R"(
    background-color: #133526;
    color: #10B981;
    border: 1.5px solid #10B981;
    border-radius: 16px;
    font-size: 26px;
    font-weight: 700;
  )");
  v_layout->addWidget(system_status_pill);

  v_layout->addSpacing(8);

  panda_status_label = new QLabel("● Panda Connected (TSS2)", card_vehicle);
  panda_status_label->setStyleSheet("font-size: 26px; font-weight: 500; color: #E2E8F0; border: none; background: transparent;");
  v_layout->addWidget(panda_status_label);

  gps_status_label = new QLabel("● GPS Locked (3D Fix)", card_vehicle);
  gps_status_label->setStyleSheet("font-size: 26px; font-weight: 500; color: #E2E8F0; border: none; background: transparent;");
  v_layout->addWidget(gps_status_label);

  auto camera_status_label = new QLabel("● Vision System Standby", card_vehicle);
  camera_status_label->setStyleSheet("font-size: 26px; font-weight: 500; color: #94A3B8; border: none; background: transparent;");
  v_layout->addWidget(camera_status_label);

  v_layout->addStretch();
  main_layout->addWidget(card_vehicle, 1);

  // ===== CARD 2: Device Health & Thermals =====
  auto card_hardware = new QFrame(this);
  card_hardware->setStyleSheet(card_style);
  auto h_layout = new QVBoxLayout(card_hardware);
  h_layout->setContentsMargins(35, 35, 35, 35);
  h_layout->setSpacing(14);

  auto h_tag = new QLabel("DEVICE HEALTH & THERMALS", card_hardware);
  h_tag->setStyleSheet("font-size: 24px; font-weight: 600; color: #8E929B; letter-spacing: 1px; border: none; background: transparent;");
  h_layout->addWidget(h_tag);

  auto temp_title = new QLabel("Chassis Ambient", card_hardware);
  temp_title->setStyleSheet("font-size: 30px; font-weight: 600; color: #E2E8F0; border: none; background: transparent;");
  h_layout->addWidget(temp_title);

  temperature_value = new QLabel("0 °C", card_hardware);
  temperature_value->setStyleSheet("font-size: 72px; font-weight: 800; color: #10B981; border: none; background: transparent;");
  h_layout->addWidget(temperature_value);

  thermal_tier_label = new QLabel("Thermal Status: Nominal", card_hardware);
  thermal_tier_label->setStyleSheet("font-size: 26px; font-weight: 600; color: #10B981; border: none; background: transparent;");
  h_layout->addWidget(thermal_tier_label);

  h_layout->addWidget(horizontal_rule(card_hardware));

  auto storage_title = new QLabel("Internal Flash Storage", card_hardware);
  storage_title->setStyleSheet("font-size: 28px; font-weight: 600; color: #E2E8F0; border: none; background: transparent;");
  h_layout->addWidget(storage_title);

  storage_value = new QLabel("Calculating...", card_hardware);
  storage_value->setStyleSheet("font-size: 34px; font-weight: 700; color: #A0A5B5; border: none; background: transparent;");
  h_layout->addWidget(storage_value);

  h_layout->addStretch();
  main_layout->addWidget(card_hardware, 1);

  // ===== CARD 3: ezpilot Software & Docs =====
  auto card_info = new QFrame(this);
  card_info->setStyleSheet(card_style);
  auto i_layout = new QVBoxLayout(card_info);
  i_layout->setContentsMargins(35, 35, 35, 35);
  i_layout->setSpacing(14);

  auto i_tag = new QLabel("SOFTWARE & SYSTEM", card_info);
  i_tag->setStyleSheet("font-size: 24px; font-weight: 600; color: #8E929B; letter-spacing: 1px; border: none; background: transparent;");
  i_layout->addWidget(i_tag);

  auto fork_title = new QLabel("ezpilot K1S", card_info);
  fork_title->setStyleSheet("font-size: 38px; font-weight: 700; color: #FFFFFF; border: none; background: transparent;");
  i_layout->addWidget(fork_title);

  version_label = new QLabel("Independent Edition", card_info);
  version_label->setStyleSheet("font-size: 26px; font-weight: 500; color: #A0A5B5; border: none; background: transparent;");
  i_layout->addWidget(version_label);

  i_layout->addWidget(horizontal_rule(card_info));

  auto branch_label = new QLabel("Branch: k1s-independent", card_info);
  branch_label->setStyleSheet("font-size: 24px; font-weight: 500; color: #8E929B; border: none; background: transparent;");
  i_layout->addWidget(branch_label);

  auto qr_container = new QWidget(card_info);
  qr_container->setStyleSheet("border: none; background: transparent;");
  auto qr_layout = new QHBoxLayout(qr_container);
  qr_layout->setContentsMargins(0, 10, 0, 0);

  auto qr_widget = new QrWidget("https://github.com/studlolox/bukapilot-k1s", card_info);
  qr_widget->setFixedSize(185, 185);
  qr_layout->addWidget(qr_widget);

  auto qr_desc = new QLabel("Scan QR for\nsource code &\ndocumentation.", card_info);
  qr_desc->setStyleSheet("font-size: 22px; font-weight: 500; color: #94A3B8; border: none; background: transparent;");
  qr_desc->setWordWrap(true);
  qr_layout->addWidget(qr_desc);

  i_layout->addWidget(qr_container);

  i_layout->addStretch();
  main_layout->addWidget(card_info, 1);

  setStyleSheet("OffroadHome { background-color: #0B0D13; }");
}

void OffroadHome::updateState(const UIState& s) {
  auto& sm = *(s.sm);

  // 1. Vehicle & System Status
  bool hasError = (s.scene.pandaType == cereal::PandaState::PandaType::UNKNOWN);
  bool initialising = hasSevereAlerts;

  if (hasError) {
    system_status_pill->setText("NO PANDA DETECTED");
    system_status_pill->setStyleSheet(R"(
      background-color: #381B1B;
      color: #EF4444;
      border: 1.5px solid #EF4444;
      border-radius: 16px;
      font-size: 24px;
      font-weight: 700;
    )");
    panda_status_label->setText("● Panda Disconnected");
    panda_status_label->setStyleSheet("font-size: 26px; font-weight: 500; color: #EF4444; border: none; background: transparent;");
  } else if (initialising) {
    system_status_pill->setText("GETTING READY");
    system_status_pill->setStyleSheet(R"(
      background-color: #3B2E15;
      color: #F59E0B;
      border: 1.5px solid #F59E0B;
      border-radius: 16px;
      font-size: 24px;
      font-weight: 700;
    )");
    panda_status_label->setText("● Panda Online (TSS2)");
    panda_status_label->setStyleSheet("font-size: 26px; font-weight: 500; color: #F59E0B; border: none; background: transparent;");
  } else {
    system_status_pill->setText("SYSTEM READY");
    system_status_pill->setStyleSheet(R"(
      background-color: #133526;
      color: #10B981;
      border: 1.5px solid #10B981;
      border-radius: 16px;
      font-size: 26px;
      font-weight: 700;
    )");
    panda_status_label->setText("● Panda Online (TSS2)");
    panda_status_label->setStyleSheet("font-size: 26px; font-weight: 500; color: #10B981; border: none; background: transparent;");
  }

  // GPS Fix
  bool gps_ok = sm["liveLocationKalman"].getLiveLocationKalman().getGpsOK();
  if (gps_ok) {
    gps_status_label->setText("● GPS Locked (3D Fix)");
    gps_status_label->setStyleSheet("font-size: 26px; font-weight: 500; color: #10B981; border: none; background: transparent;");
  } else {
    gps_status_label->setText("● GPS Searching Satellite...");
    gps_status_label->setStyleSheet("font-size: 26px; font-weight: 500; color: #94A3B8; border: none; background: transparent;");
  }

  // Vehicle model display
  std::string car_name = params.get("CarModel");
  if (!car_name.empty()) {
    vehicle_title->setText(QString::fromStdString(car_name));
  } else {
    vehicle_title->setText("Toyota Corolla Cross");
  }

  // 2. Hardware & Thermals
  auto deviceState = sm["deviceState"].getDeviceState();
  float ambient = deviceState.getAmbientTempC();
  temperature_value->setText(QString::number((int)std::round(ambient)) + " °C");

  auto ts = deviceState.getThermalStatus();
  if (ts == cereal::DeviceState::ThermalStatus::GREEN) {
    temperature_value->setStyleSheet("font-size: 72px; font-weight: 800; color: #10B981; border: none; background: transparent;");
    thermal_tier_label->setText("Thermal Status: Nominal");
    thermal_tier_label->setStyleSheet("font-size: 26px; font-weight: 600; color: #10B981; border: none; background: transparent;");
  } else if (ts == cereal::DeviceState::ThermalStatus::YELLOW) {
    temperature_value->setStyleSheet("font-size: 72px; font-weight: 800; color: #F59E0B; border: none; background: transparent;");
    thermal_tier_label->setText("Thermal Status: Warm (Cooling Active)");
    thermal_tier_label->setStyleSheet("font-size: 26px; font-weight: 600; color: #F59E0B; border: none; background: transparent;");
  } else {
    temperature_value->setStyleSheet("font-size: 72px; font-weight: 800; color: #EF4444; border: none; background: transparent;");
    thermal_tier_label->setText("Thermal Status: Overheat Prevention");
    thermal_tier_label->setStyleSheet("font-size: 26px; font-weight: 600; color: #EF4444; border: none; background: transparent;");
  }

  // Storage
  int free_percent = (int)deviceState.getFreeSpacePercent();
  storage_value->setText(QString::number(free_percent) + "% Available");

  // Version
  version_label->setText(getBrandVersion());
}
