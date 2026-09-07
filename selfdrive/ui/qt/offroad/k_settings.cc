#include "selfdrive/ui/qt/offroad/k_settings.h"

#include <cassert>
#include <cmath>
#include <string>

#include <QDebug>

#ifndef QCOM
#include "selfdrive/ui/qt/offroad/networking.h"
#endif

#include "selfdrive/common/params.h"
#include "selfdrive/common/util.h"
#include "selfdrive/hardware/hw.h"
#include "selfdrive/ui/qt/widgets/controls.h"
#include "selfdrive/ui/qt/widgets/popup.h"
#include "selfdrive/ui/qt/widgets/input.h"
#include "selfdrive/ui/qt/widgets/scrollview.h"
#include "selfdrive/ui/qt/widgets/ssh_keys.h"
#include "selfdrive/ui/qt/widgets/toggle.h"
#include "selfdrive/ui/ui.h"
#include "selfdrive/ui/qt/util.h"
#include "selfdrive/ui/qt/qt_window.h"

TogglesPanel::TogglesPanel(SettingsWindow *parent) : ListWidget(parent) {
  // param, title, desc, icon, restart_required
  std::vector<std::tuple<QString, QString, QString, QString, bool>> toggles{
    {
      "OpenpilotEnabledToggle",
      "Enable ezpilot",
      "Use the ezpilot system for adaptive cruise control and lane keep driver assistance. Your attention is required at all times to use this feature. Changing this setting takes effect when the car is powered off.",
      "../assets/kommu/icon_ezpilot.png",
      true,
    },
    {
      "IsLdwEnabled",
      "Enable Lane Departure Warnings",
      "Receive alerts to steer back into the lane when your vehicle drifts over a detected lane line without a turn signal activated while driving over 31 mph (50 km/h).",
      "../assets/kommu/icon_warning.png",
      true,
    },
    {
      "IsAlcEnabled",
      "Enable Assisted Lane Change",
      "Assisted Lane Change will assist your vehicle in a single lane change when a steering nudge and the vehicle's signal lights are turned on. This features works over 31mph (50 km/h).",
      "../assets/kommu/icon_ezpilot.png",
      true,
    },
    {
      "IsRHD",
      "Enable Right-Hand Drive",
      "Allow ezpilot to obey left-hand traffic conventions and perform driver monitoring on right driver seat.",
      "../assets/kommu/icon_ezpilot_mirrored.png",
      true,
    },
    {
      "QuietMode",
      "Quiet Mode",
      "Receive only safety critical alerts.",
      "../assets/kommu/quiet_mode.png",
      false,
    },
    {
      "LogVideoWifiOnly",
      "Upload Drive Video via Wi-Fi Only",
      "Enable upload of on-the-road driving footage via Wi-Fi only, mobile data will not be used for uploading driving footage.",
      "../assets/offroad/icon_road.png",
      false,
    },
    {
      "DisableVideoRecording",
      "Disable Road Video Recording",
      "Disable recording road camera footage to internal storage. Significantly reduces device temperature, power draw, and flash memory wear while preserving all driving assistance features.",
      "../assets/offroad/icon_road.png",
      true,
    },
    {
      "DisableRadar",
      "ezpilot Longitudinal Control",
      "ezpilot will disable the car's radar and will take over control of gas and brakes. Warning: this disables AEB!",
      "../assets/offroad/icon_speed_limit.png",
      true,
    },
    {
      "UseStockAcc",
      "Stock Longitudinal Control",
      "ezpilot will use the stock ACC instead of ezpilot's ACC.",
      "../assets/offroad/icon_speed_limit.png",
      false,
    },
    {
      "ResumeWithRes",
      "Resume from Stop with RES+",
      "Require pressing the steering wheel RES+ button or tapping the accelerator to resume driving from a complete stop. When disabled, the car automatically resumes when the vehicle ahead moves.",
      "../assets/offroad/icon_speed_limit.png",
      false,
    },
    {
      "ScreenOffDriving",
      "Screen Off While Driving",
      "Turn off the screen after 15 seconds of driving with no interaction. The screen automatically wakes up when tapped or when a driving alert occurs.",
      "../assets/offroad/icon_shell.png",
      false,
    },
  };

  Params params;

  for (auto &[param, title, desc, icon, restart_req] : toggles) {
    auto toggle = new ParamControl(param, title, desc, icon, this, restart_req);
    bool locked = params.getBool((param + "Lock").toStdString());
    toggle->setEnabled(!locked);
    if (!locked) {
      unlocked_toggles.push_back(toggle);
    }
    addItem(toggle);
  }

  auto dm_toggle = new ConfirmParamControl(
    "IgnoreDM",
    "Disable Driver Monitoring",
    "Ignore driver monitoring alerts and disengagements. WARNING: This disables driver attentiveness checks and will not warn or disengage when you are distracted.",
    "../assets/offroad/icon_monitoring.png",
    "WARNING: Disabling Driver Monitoring disables attentiveness checks!\n\nAre you sure you want to proceed?",
    this);
  bool dm_locked = params.getBool("IgnoreDMLock");
  dm_toggle->setEnabled(!dm_locked);
  if (!dm_locked) {
    unlocked_toggles.push_back(dm_toggle);
  }
  addItem(dm_toggle);

  std::string vtsc_val = params.get("VisionTurnSpeedControl");
  if (vtsc_val.empty()) {
    vtsc_val = "1"; // Default to E2E Only
    params.put("VisionTurnSpeedControl", vtsc_val);
  }
  QString vtsc_text = "E2E Only";
  if (vtsc_val == "0") vtsc_text = "Off";
  else if (vtsc_val == "2") vtsc_text = "Always On";

  vtscBtn = new ButtonControl(
    "Vision Turn Speed Control (VTSC)",
    vtsc_text,
    "Automatically decelerates ahead of sharp turns and highway curves using vision AI curvature prediction. Choose Off, E2E Only, or Always On.");
  QObject::connect(vtscBtn, &ButtonControl::clicked, [=]() {
    std::string cur = Params().get("VisionTurnSpeedControl");
    std::string next_val = "1";
    QString next_text = "E2E Only";
    if (cur == "1") {
      next_val = "2";
      next_text = "Always On";
    } else if (cur == "2") {
      next_val = "0";
      next_text = "Off";
    } else {
      next_val = "1";
      next_text = "E2E Only";
    }
    Params().put("VisionTurnSpeedControl", next_val);
    vtscBtn->setText(next_text);
  });
  addItem(vtscBtn);

  connect(uiState(), &UIState::uiUpdate, this, &TogglesPanel::updateState);
}

void TogglesPanel::showEvent(QShowEvent *event) {
  bool car_moving = uiState()->carMoving();
  car_moving_prev = car_moving;
  for (auto toggle : unlocked_toggles) {
    toggle->setEnabled(!car_moving);
  }
  if (vtscBtn) {
    std::string cur = Params().get("VisionTurnSpeedControl");
    if (cur == "0") vtscBtn->setText("Off");
    else if (cur == "2") vtscBtn->setText("Always On");
    else vtscBtn->setText("E2E Only");
  }
  ListWidget::showEvent(event);
}

void TogglesPanel::updateState(const UIState &s) {
  bool car_moving = s.carMoving();
  if (car_moving != car_moving_prev) {
    car_moving_prev = car_moving;
    for (auto toggle : unlocked_toggles) {
      toggle->setEnabled(!car_moving);
    }
  }
}

DevicePanel::DevicePanel(SettingsWindow *parent) : ListWidget(parent) {
  addItem(new LabelControl("Dongle ID", getDongleId().value_or("N/A")));
  serialBtn = new ButtonControl("Serial", params.get("HardwareSerial").c_str(), "", true);
  addItem(serialBtn);
  testBtn = new ButtonControl("QC Test", "Start");
  replaceSplashBtn = new ButtonControl("Replace Splash Image", "Replace");
  dumpTmuxBtn = new ButtonControl("Dump TMUX", "Dump");

  // offroad-only buttons
  auto dcamBtn = new ButtonControl("Driver Camera", "PREVIEW",
                                   "Preview the driver facing camera to help optimize device mounting position for best driver monitoring experience. (vehicle must be off)");
  connect(dcamBtn, &ButtonControl::clicked, [=]() { emit showDriverView(); });
  addItem(dcamBtn);

  resetCalibBtn = new ButtonControl("Reset Calibration", "RESET", " ");
  connect(resetCalibBtn, &ButtonControl::showDescription, this, &DevicePanel::updateCalibDescription);
  connect(resetCalibBtn, &ButtonControl::clicked, [&]() {
    if (ConfirmationDialog::confirm("Are you sure to\nReset Calibration?", this)) {
      params.remove("CalibrationParams");
    }
  });
  addItem(resetCalibBtn);

  connect(serialBtn, &ButtonControl::clicked, [=]() {
    dev_tab_counter++;
    if (dev_tab_counter == 3) {
      addItem(testBtn);
      addItem(replaceSplashBtn);
      addItem(dumpTmuxBtn);

      connect(replaceSplashBtn, &ButtonControl::clicked, [=]() {
        std::system("dd if=/data/openpilot/selfdrive/assets/newsplash.img of=/dev/block/bootdevice/by-name/splash");
      });
      connect(dumpTmuxBtn, &ButtonControl::clicked, [=]() {
        QString output = exec("tmux capture-pane -pS -1000 | nc termbin.com 9999").c_str();
        Popup("Termbin URL", output, Popup::OK, this).exec();
      });


      connect(testBtn, &ButtonControl::clicked, [=]() {
        std::string filename = "_report";

        if ( access( filename.c_str(), F_OK ) != -1 ) {
          QString test_output = exec("cat _report").c_str();
          std::system("rm _report");
          std::system("rm -rf /data/media/0/realdata/");
          testBtn->setText("Restart");
          Popup("QC Report", test_output, Popup::OK, this).exec();
        }
        else {
          if (!params.getBool("IsOffroad")) {
            ConfirmationDialog::alert("Ensure ignition is off first!", this);
          }
          else {
            if (ConfirmationDialog::confirm("Spoof calibration. Proceed?", this)) {
              exec("/data/openpilot/selfdrive/test/qc_test.py -v &> _report &");
              testBtn->setText("Report");
            }
          }
        }
      });
    }
  });

  if (Hardware::TICI()) {
    auto regulatoryBtn = new ButtonControl("Regulatory", "VIEW", "");
    connect(regulatoryBtn, &ButtonControl::clicked, [=]() {
      const std::string txt = util::read_file("../assets/offroad/fcc.html");
      RichTextDialog::alert(QString::fromStdString(txt), this);
    });
    addItem(regulatoryBtn);
  }

  QObject::connect(uiState(), &UIState::offroadTransition, [=](bool offroad) {
    for (auto btn : findChildren<ButtonControl *>()) {
      btn->setEnabled(offroad);
    }
    resetCalibBtn->setEnabled(true);
  });

  // power buttons
  QHBoxLayout *power_layout = new QHBoxLayout();
  power_layout->setSpacing(30);

  QPushButton *reboot_btn = new QPushButton("Reboot");
  reboot_btn->setObjectName("reboot_btn");
  power_layout->addWidget(reboot_btn);
  QObject::connect(reboot_btn, &QPushButton::clicked, this, &DevicePanel::reboot);

  QPushButton *poweroff_btn = new QPushButton("Power Off");
  poweroff_btn->setObjectName("poweroff_btn");
  power_layout->addWidget(poweroff_btn);
  QObject::connect(poweroff_btn, &QPushButton::clicked, this, &DevicePanel::poweroff);

  if (Hardware::TICI()) {
    connect(uiState(), &UIState::offroadTransition, poweroff_btn, &QPushButton::setVisible);
  }

  setStyleSheet(R"(
    #reboot_btn { height: 120px; border-radius: 15px; background-color: #393939; }
    #reboot_btn:pressed { background-color: #4a4a4a; }
    #poweroff_btn { height: 120px; border-radius: 15px; background-color: #E22C2C; }
    #poweroff_btn:pressed { background-color: #FF2424; }
  )");
  addItem(power_layout);
}

void DevicePanel::updateCalibDescription() {
  QString desc =
      "ezpilot requires the device to be mounted within 4° left or right and "
      "within 5° up or 8° down. ezpilot is continuously calibrating, resetting is rarely required.";
  std::string calib_bytes = Params().get("CalibrationParams");
  if (!calib_bytes.empty()) {
    try {
      AlignedBuffer aligned_buf;
      capnp::FlatArrayMessageReader cmsg(aligned_buf.align(calib_bytes.data(), calib_bytes.size()));
      auto calib = cmsg.getRoot<cereal::Event>().getLiveCalibration();
      if (calib.getCalStatus() != 0) {
        double pitch = calib.getRpyCalib()[1] * (180 / M_PI);
        double yaw = calib.getRpyCalib()[2] * (180 / M_PI);
        desc += QString(" Your device is pointed %1° %2 and %3° %4.")
                    .arg(QString::number(std::abs(pitch), 'g', 1), pitch > 0 ? "down" : "up",
                         QString::number(std::abs(yaw), 'g', 1), yaw > 0 ? "left" : "right");
      }
    } catch (kj::Exception) {
      qInfo() << "invalid CalibrationParams";
    }
  }
  qobject_cast<ButtonControl *>(sender())->setDescription(desc);
}

void DevicePanel::reboot() {
  if (!uiState()->engaged()) {
    if (ConfirmationDialog::confirm("Are you sure to\nReboot?", this)) {
      // Check engaged again in case it changed while the dialog was open
      if (!uiState()->engaged()) {
        Params().putBool("DoReboot", true);
      }
    }
  } else {
    ConfirmationDialog::alert("Disengage to Reboot", this);
  }
}

void DevicePanel::poweroff() {
  if (!uiState()->engaged()) {
    if (ConfirmationDialog::confirm("Are you sure to\nPower Off?", this)) {
      // Check engaged again in case it changed while the dialog was open
      if (!uiState()->engaged()) {
        Params().putBool("DoShutdown", true);
      }
    }
  } else {
    ConfirmationDialog::alert("Disengage to Power Off", this);
  }
}

PersonalisedPanel::PersonalisedPanel(QWidget* parent) : ListWidget(parent) {
  // min, max, step
  stopDistanceOffsetSb = new SpinboxControl("StoppingDistanceOffset","Stop Distance Offset", "The offset distance from the lead car the vehicle is meant to stop", "m", (double []){-2.0, 5.0, 0.1}, true);
  addItem(stopDistanceOffsetSb);

  drivePathOffsetSb = new SpinboxControl("DrivePathOffset","Path Skew Offset", "The path offset from center of the lane. Perform positive offset if the vehicle is currently skewed left.", "m", (double []){-1.0, 1.0, 0.05}, true);
  addItem(drivePathOffsetSb);

  fanPwmOverrideSb = new SpinboxControl("FanPwmOverride","Fan Speed", "Note: Lowering the fan speed may reduce the overall fan noise but risk of device overheating.", "%", (double []){0, 100.0, 10.0}, false);
  addItem(fanPwmOverrideSb);

  powerSaverEntryDurationSb = new SpinboxControl("PowerSaverEntryDuration","Device Poweroff", "Power saver entry duration after ignition is off.", "min", (double []){10, 720.0, 10.0}, false);
  addItem(powerSaverEntryDurationSb);

  connect(uiState(), &UIState::uiUpdate, this, &PersonalisedPanel::updateState);
}

void PersonalisedPanel::showEvent(QShowEvent *event) {
  bool car_moving = uiState()->carMoving();
  car_moving_prev = car_moving;
  stopDistanceOffsetSb->setEnabled(!car_moving);
  drivePathOffsetSb->setEnabled(!car_moving);
  fanPwmOverrideSb->setEnabled(!car_moving);
  powerSaverEntryDurationSb->setEnabled(!car_moving);
  ListWidget::showEvent(event);
}

void PersonalisedPanel::updateState(const UIState &s) {
  bool car_moving = s.carMoving();
  if (car_moving != car_moving_prev) {
    car_moving_prev = car_moving;
    stopDistanceOffsetSb->setEnabled(!car_moving);
    drivePathOffsetSb->setEnabled(!car_moving);
    fanPwmOverrideSb->setEnabled(!car_moving);
    powerSaverEntryDurationSb->setEnabled(!car_moving);
  }
}

SoftwarePanel::SoftwarePanel(QWidget* parent) : ListWidget(parent) {
  gitCommitLbl = new LabelControl("Git Commit");
  osVersionLbl = new LabelControl("OS Version");
  versionLbl = new LabelControl("Version", "", getVersion());
  lastUpdateLbl = new LabelControl("Last Update Status", "", "The status ezpilot last checked for an update. The updater only runs while the car is off.");
  updateBtn = new ButtonControl("Check for Update", "");
  featuresInput = new FeaturesControl();
  fingerprintInput = new FixFingerprintSelect();
  branchInput = new ChangeBranchSelect();

  connect(updateBtn, &ButtonControl::clicked, [=]() {
    if (params.getBool("IsOffroad")) {
      fs_watch->addPath(QString::fromStdString(params.getParamPath("LastUpdateTime")));
      fs_watch->addPath(QString::fromStdString(params.getParamPath("UpdateFailedCount")));
      fs_watch->addPath(QString::fromStdString(params.getParamPath("UpdateStatus")));
      params.put("UpdateStatus", "checking");
      updateLabels();
    }
    std::system("pkill -1 -f selfdrive.updated");
  });

  QWidget *widgets[] = {versionLbl, lastUpdateLbl, updateBtn, gitCommitLbl, osVersionLbl, featuresInput, fingerprintInput, branchInput};
  for (QWidget* w : widgets) {
    addItem(w);
  }

  fs_watch = new QFileSystemWatcher(this);
  QObject::connect(fs_watch, &QFileSystemWatcher::fileChanged, [=](const QString path) {
    updateLabels();
  });

  timer = new QTimer(this);
  timer->callOnTimeout(this, &SoftwarePanel::updateLabels);
}

void SoftwarePanel::showEvent(QShowEvent *event) {
  updateLabels();
  timer->start(1000);
}

void SoftwarePanel::hideEvent(QHideEvent *event) {
  timer->stop();
}

void SoftwarePanel::updateLabels() {
  QString lastUpdate = QString();
  QString btnText = "CHECK";
  bool allowed = false;
  std::string status = params.get("UpdateStatus");
  auto tm = params.get("LastUpdateTime");
  if (std::stoi(util::check_output("date +%Y")) < 2022) {
    lastUpdate = "Invalid date and time settings";
  } else if (not params.getBool("IsOffroad")) {
    lastUpdate = "Turn off the car to check for update";
  } else if (status == "unsavedChanges") {
    lastUpdate = "Changes unsaved, cannot update";
    allowed = true;
  } else if (status == "waiting") {
    allowed = true;
  } else if (status == "success") {
    lastUpdate = "Successful, reboot to apply update";
    btnText = "REBOOT";
    allowed = params.getBool("IsOffroad");
    connect(updateBtn, &ButtonControl::clicked, [=]() {
      updateBtn->setText("REBOOTING");
      updateBtn->setEnabled(false);
      Params().putBool("DoReboot", true);
    });
  } else if (status == "checking") {
    btnText = "CHECKING";
    lastUpdate = "Checking for update";
  } else if (status == "prepareDownload") {
    btnText = "UPDATING";
    lastUpdate = "Preparing to download update";
  } else if (status == "downloading") {
    btnText = "UPDATING";
    lastUpdate = "Downloading update";
  } else if (status == "installing") {
    btnText = "UPDATING";
    lastUpdate = "Installing update";
  } else if (status == "fetchFailed") {
    lastUpdate = "Failed to fetch update";
    allowed = true;
  } else if (!tm.empty()) {
    lastUpdate = "Checked " + timeAgo(QDateTime::fromString(QString::fromStdString(tm + "Z"), Qt::ISODate));
    allowed = true;
    if (status == "noInternet") {
      lastUpdate += ", no internet";
    } else if (status == "latest") {
      lastUpdate += ", up to date";
    } else {
      lastUpdate = QString();
    }
  }

  versionLbl->setText(getVersion());
  lastUpdateLbl->setText(lastUpdate);
  updateBtn->setText(btnText);
  updateBtn->setEnabled(allowed);
  gitCommitLbl->setText(QString::fromStdString(params.get("GitCommit")).left(10));
  osVersionLbl->setText(QString::fromStdString(Hardware::get_os_version()).trimmed());
}

C2NetworkPanel::C2NetworkPanel(QWidget *parent) : ListWidget(parent) {
  // wifi + tethering buttons
#ifdef QCOM
  auto wifiBtn = new ButtonControl("Wi-Fi Settings", "OPEN");
  connect(wifiBtn, &ButtonControl::clicked, [=]() { HardwareEon::launch_wifi(); });
  addItem(wifiBtn);

  auto tetheringBtn = new ButtonControl("Tethering Settings", "OPEN");
  connect(tetheringBtn, &ButtonControl::clicked, [=]() { HardwareEon::launch_tethering(); });
  addItem(tetheringBtn);
#endif
  ipaddress = new LabelControl("IP Address", "");
  addItem(ipaddress);

  networkType = new LabelControl("Network Type", "");
  addItem(networkType);

  // SSH key management
  addItem(new SshToggle());
  addItem(new SshControl());

  timer = new QTimer(this);
  timer->callOnTimeout(this, &C2NetworkPanel::updateLabels);
}

void C2NetworkPanel::showEvent(QShowEvent *event) {
  updateLabels();
  timer->start(1500);
}

void C2NetworkPanel::hideEvent(QHideEvent *event) {
  timer->stop();
}

void C2NetworkPanel::updateLabels() {
  ipaddress->setText(getIPAddress());
  networkType->setText(getNetworkType());
}

QString C2NetworkPanel::getIPAddress() {
  std::string result = util::check_output("ifconfig wlan0");
  if (result.empty()) return "";

  const std::string inetaddrr = "inet addr:";
  std::string::size_type begin = result.find(inetaddrr);
  if (begin == std::string::npos) return "";

  begin += inetaddrr.length();
  std::string::size_type end = result.find(' ', begin);
  if (end == std::string::npos) return "";

  return result.substr(begin, end - begin).c_str();
}

QString C2NetworkPanel::getNetworkType() {
  const QMap<cereal::DeviceState::NetworkType, QString> network_type = {
    {cereal::DeviceState::NetworkType::NONE, "None"},
    {cereal::DeviceState::NetworkType::WIFI, "Wi-Fi"},
    {cereal::DeviceState::NetworkType::ETHERNET, "ETH"},
    {cereal::DeviceState::NetworkType::CELL2_G, "2G"},
    {cereal::DeviceState::NetworkType::CELL3_G, "3G"},
    {cereal::DeviceState::NetworkType::CELL4_G, "4G"},
    {cereal::DeviceState::NetworkType::CELL5_G, "5G"}
  };
  auto &sm = *(uiState()->sm);
  return network_type[sm["deviceState"].getDeviceState().getNetworkType()];
}

QWidget *network_panel(QWidget *parent) {
#ifdef QCOM
  return new C2NetworkPanel(parent);
#else
  return new Networking(parent);
#endif
}

void SettingsWindow::showEvent(QShowEvent *event) {
  panel_widget->setCurrentIndex(0);
  nav_btns->buttons()[0]->setChecked(true);
}

SettingsWindow::SettingsWindow(QWidget *parent) : QFrame(parent) {

  // setup two main layouts
  sidebar_widget = new QWidget;
  QVBoxLayout *sidebar_layout = new QVBoxLayout(sidebar_widget);
  sidebar_layout->setMargin(0);
  panel_widget = new QStackedWidget();
  panel_widget->setStyleSheet(R"(
    border-radius: 1px;
    background-color: #202020;
  )");

  // close button
  QPushButton *close_btn = new QPushButton("×");
  close_btn->setStyleSheet(R"(
    QPushButton {
      font-size: 90px;
      padding-bottom: 20px;
      border: 0px black solid;
      border-radius: 75px;
      background-color: black;
    }
    QPushButton:pressed {
      background-color: #3B3B3B;
    }
  )");
  close_btn->setFixedSize(100, 100);
  sidebar_layout->addWidget(close_btn, 0, Qt::AlignTop);
  QObject::connect(close_btn, &QPushButton::clicked, this, &SettingsWindow::closeSettings);

  // setup panels
  DevicePanel *device = new DevicePanel(this);
  QObject::connect(device, &DevicePanel::showDriverView, this, &SettingsWindow::showDriverView);

  struct NavItem {
    QString name;
    QPixmap icon;
    QWidget *w;
  };

  std::vector<NavItem> panels = {
    {"   Device", loadPixmap("../assets/kommu/device.png", {60, 60}), device},
    {"   Network", loadPixmap("../assets/kommu/network.png", {60, 60}), network_panel(this)},
    {"   Toggles", loadPixmap("../assets/kommu/toggles.png", {60, 60}), new TogglesPanel(this)},
    {"   Personalised", loadPixmap("../assets/kommu/personalised.png", {60, 60}), new PersonalisedPanel(this)},
    {"   Software", loadPixmap("../assets/kommu/software.png", {60, 60}), new SoftwarePanel(this)},
  };

  const int padding = 55;

  nav_btns = new QButtonGroup(this);
  for (auto &[name, icon, panel] : panels) {
    auto btn = new QPushButton(icon, name);
    btn->setCheckable(true);
    btn->setChecked(nav_btns->buttons().size() == 0);
    btn->setFixedHeight(175);
    btn->setStyleSheet(QString(R"(
      QPushButton {
        color: grey;
        border: none;
        background: none;
        font-size: 50px;
        font-weight: 500;
        padding-top: 50px;
        padding-bottom: %1px;
      }
      QPushButton:checked {
        color: white;
      }
      QPushButton:pressed {
        color: #ADADAD;
      }
    )").arg(padding));

    nav_btns->addButton(btn);
    sidebar_layout->addWidget(btn, 0, Qt::AlignLeft);

    const int lr_margin = name != "Network" ? 50 : 0;  // Network panel handles its own margins
    panel->setContentsMargins(lr_margin, 25, lr_margin, 25);

    ScrollView *panel_frame = new ScrollView(panel, this);
    panel_widget->addWidget(panel_frame);

    QObject::connect(btn, &QPushButton::clicked, [=, w = panel_frame]() {
      btn->setChecked(true);
      panel_widget->setCurrentWidget(w);
    });
  }
  sidebar_layout->setSpacing(75);
  sidebar_layout->setContentsMargins(25, 50, 100, 50);

  // main settings layout, sidebar + main panel
  QHBoxLayout *main_layout = new QHBoxLayout(this);

  sidebar_widget->setFixedWidth(500);
  main_layout->addWidget(sidebar_widget);
  main_layout->addWidget(panel_widget);

  setStyleSheet(R"(
    * {
      color: white;
      font-size: 50px;
    }
    SettingsWindow {
      background-color: black;
    }
  )");
}

void SettingsWindow::hideEvent(QHideEvent *event) {
#ifdef QCOM
  HardwareEon::close_activities();
#endif
}
