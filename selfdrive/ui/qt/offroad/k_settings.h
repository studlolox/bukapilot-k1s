#pragma once

#include <vector>
#include <QButtonGroup>
#include <QFileSystemWatcher>
#include <QFrame>
#include <QLabel>
#include <QHBoxLayout>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "selfdrive/common/features.h"
#include "selfdrive/common/params.h"
#include "selfdrive/ui/qt/widgets/controls.h"
#include "selfdrive/ui/qt/widgets/input.h"
#include "selfdrive/common/util.h"

// ********** settings window + top-level panels **********
class SettingsWindow : public QFrame {
  Q_OBJECT

public:
  explicit SettingsWindow(QWidget *parent = 0);

protected:
  void hideEvent(QHideEvent *event) override;
  void showEvent(QShowEvent *event) override;

signals:
  void closeSettings();
  void showDriverView();

private:
  QPushButton *sidebar_alert_widget;
  QWidget *sidebar_widget;
  QButtonGroup *nav_btns;
  QStackedWidget *panel_widget;
};

class DevicePanel : public ListWidget {
  Q_OBJECT
public:
  explicit DevicePanel(SettingsWindow *parent);
signals:
  void showDriverView();

private slots:
  void poweroff();
  void reboot();
  void updateCalibDescription();

private:
  ButtonControl *resetCalibBtn;
  ButtonControl *serialBtn;
  ButtonControl *testBtn;
  ButtonControl *replaceSplashBtn;
  ButtonControl *dumpTmuxBtn;
  SpinboxControl *stopDistanceOffsetSb;
  SpinboxControl *drivePathOffsetSb;
  SpinboxControl *fanPwmOverrideSb;
  SpinboxControl *powerSaverEntryDurationSb;

  int dev_tab_counter = 0;
  Params params;
};

class TogglesPanel : public ListWidget {
  Q_OBJECT
public:
  explicit TogglesPanel(SettingsWindow *parent);

protected:
  void showEvent(QShowEvent *event) override;

public slots:
  void updateState(const UIState &s);

private:
  std::vector<ToggleControl *> unlocked_toggles;
  ButtonControl *vtscBtn = nullptr;
  bool car_moving_prev = false;
};

class PersonalisedPanel : public ListWidget {
  Q_OBJECT
public:
  explicit PersonalisedPanel(QWidget* parent = nullptr);

protected:
  void showEvent(QShowEvent *event) override;

public slots:
  void updateState(const UIState &s);

private:
  SpinboxControl *stopDistanceOffsetSb;
  SpinboxControl *drivePathOffsetSb;
  SpinboxControl *fanPwmOverrideSb;
  SpinboxControl *powerSaverEntryDurationSb;
  bool car_moving_prev = false;
};

class ConfirmParamControl : public ToggleControl {
  Q_OBJECT

public:
  ConfirmParamControl(const QString &param, const QString &title, const QString &desc, const QString &icon,
                      const QString &confirm_prompt, QWidget *parent = nullptr)
      : ToggleControl(title, desc, icon, false, parent), confirm_prompt(confirm_prompt) {
    key = param.toStdString();
    QObject::connect(this, &ToggleControl::toggleFlipped, [=](bool state) {
      if (state) {
        if (ConfirmationDialog::confirm(this->confirm_prompt, this)) {
          params.putBool(key, true);
        } else {
          toggle.togglePosition();
        }
      } else {
        params.putBool(key, false);
      }
    });
  }

  void showEvent(QShowEvent *event) override {
    if (params.getBool(key) != toggle.on) {
      toggle.togglePosition();
    }
  }

private:
  std::string key;
  QString confirm_prompt;
  Params params;
};

class FeaturesControl : public ButtonControl {
  Q_OBJECT

public:
  FeaturesControl() : ButtonControl("Features Package", "EDIT", "Warning: Only use under guidance of a support staff.") {
    package_label = new QLabel();
    package_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    package_label->setStyleSheet("color: #aaaaaa");
    setElidedText(package_label, Params().get("FeaturesPackage").c_str());
    hlayout->insertWidget(1, package_label);
    connect(this, &ButtonControl::clicked, [=] {
      InputDialog dialog("Enter Feature Package Names", this,
      "Feature package names are separated by commas ( \u2009<b>,</b>\u2009 ).<br>Empty to use the default feature package.");
      dialog.setMinLength(0);
      QString currentFeatures = Params().get("FeaturesPackage").c_str();
      if (currentFeatures != "default") dialog.updateDefaultText(currentFeatures + ", ");
      if (dialog.exec() == QDialog::Accepted) {
        QString packageText = dialog.text().trimmed();
        if (Features().set_package(packageText.toStdString()) == -1) ConfirmationDialog::alert("\nSome feature package names\nare invalid and were not added.", this);
        setElidedText(package_label, Params().get("FeaturesPackage").c_str());
      }
    });
  }

private:
  QLabel *package_label;
};

class FixFingerprintDialog : public QDialogBase {
public:
  explicit FixFingerprintDialog(QWidget *parent) : QDialogBase(parent) {
    QFrame *container = new QFrame(this);
    container->setStyleSheet("QFrame { border-radius: 16px; background-color: #12161E; border: 1.5px solid #222938; }");
    auto main_layout = new QVBoxLayout(container);
    main_layout->setContentsMargins(40, 36, 40, 36);
    main_layout->setSpacing(16);

    auto title = new QLabel("Select Vehicle Fingerprint", this);
    title->setStyleSheet("font-size: 48px; font-weight: 700; color: #FFFFFF; border: none; background: transparent;");
    main_layout->addWidget(title, 0, Qt::AlignHCenter);

    auto subtitle = new QLabel("Choose a Corolla Cross TSS2 preset, clear to auto-detect, or enter custom.", this);
    subtitle->setStyleSheet("font-size: 28px; font-weight: 500; color: #94A3B8; border: none; background: transparent;");
    subtitle->setWordWrap(true);
    subtitle->setAlignment(Qt::AlignCenter);
    main_layout->addWidget(subtitle, 0, Qt::AlignHCenter);

    main_layout->addSpacing(8);

    auto add_option = [&](const QString &label_text, const QString &val, bool is_custom = false) {
      auto btn = new QPushButton(label_text, this);
      btn->setStyleSheet(R"(
        QPushButton {
          height: 100px;
          font-size: 34px;
          font-weight: 600;
          border-radius: 14px;
          color: #FFFFFF;
          background-color: #1E2536;
          border: 1.5px solid #334155;
          text-align: center;
          padding: 6px 16px;
        }
        QPushButton:pressed {
          background-color: #0284C7;
          border-color: #38BDF8;
        }
      )");
      connect(btn, &QPushButton::clicked, [this, val, is_custom]() {
        if (is_custom) {
          QString custom = InputDialog::getText("Enter Car Model", this).trimmed();
          if (!custom.isEmpty()) {
            selected_model = custom;
            action_chosen = 1;
            accept();
          }
        } else {
          selected_model = val;
          action_chosen = 1;
          accept();
        }
      });
      main_layout->addWidget(btn);
    };

    add_option("🚗  Toyota Corolla Cross (Petrol TSS2)", "TOYOTA COROLLA CROSS");
    add_option("⚡  Toyota Corolla Cross Hybrid (TSS2)", "TOYOTA COROLLA CROSS HYBRID");
    add_option("🔍  Auto Detect (Clear Preset)", "");
    add_option("✏️  Custom Model Name...", "", true);

    main_layout->addSpacing(6);

    auto cancel_btn = new QPushButton("Cancel", this);
    cancel_btn->setStyleSheet(R"(
      QPushButton {
        height: 80px;
        font-size: 30px;
        font-weight: 500;
        border-radius: 12px;
        color: #94A3B8;
        background-color: #0F172A;
        border: 1.5px solid #1E293B;
      }
      QPushButton:pressed {
        background-color: #1E293B;
      }
    )");
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    main_layout->addWidget(cancel_btn);

    auto outer_layout = new QVBoxLayout(this);
    outer_layout->setContentsMargins(100, 60, 100, 60);
    outer_layout->addWidget(container);
  }

  QString selected_model = "";
  int action_chosen = 0;
};

class FixFingerprintSelect : public ButtonControl {
  Q_OBJECT

public:
  FixFingerprintSelect() : ButtonControl("Fix Fingerprint", "SET", "Warning: Selecting the wrong car fingerprint can be dangerous!") {
    selection_label = new QLabel();
    selection_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    selection_label->setStyleSheet("font-size: 32px; font-weight: 500; color: #94A3B8;");
    refreshLabel();
    hlayout->insertWidget(1, selection_label);
    connect(this, &ButtonControl::clicked, [=] {
      FixFingerprintDialog dlg(this);
      if (dlg.exec() == QDialog::Accepted && dlg.action_chosen == 1) {
        std::string chosen = dlg.selected_model.toStdString();
        if (chosen.empty()) {
          Params().remove("FixFingerprint");
        } else {
          Params().put("FixFingerprint", chosen);
        }
        Params().remove("CarParamsCache");
        refreshLabel();
      }
    });
  }

  void refreshLabel() {
    std::string current_fp = Params().get("FixFingerprint");
    if (current_fp.empty()) {
      setElidedText(selection_label, "Auto Detect");
      selection_label->setStyleSheet("font-size: 32px; font-weight: 500; color: #10B981;");
    } else {
      setElidedText(selection_label, current_fp.c_str());
      selection_label->setStyleSheet("font-size: 32px; font-weight: 500; color: #38BDF8;");
    }
  }

private:
  QLabel *selection_label;
};

class ChangeBranchSelect : public ButtonControl {
  Q_OBJECT

public:
  ChangeBranchSelect() : ButtonControl("Change Branch", "SET", "Warning: Untested branches may cause unexpected behaviours.") {
    selection_label = new QLabel();
    selection_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    selection_label->setStyleSheet("color: #aaaaaa");
    // Get the current branch name
    std::string currentBranchName = util::check_output("git symbolic-ref --short HEAD");
    currentBranchName.erase(currentBranchName.find_last_not_of("\n") + 1); // Remove line feed
    setElidedText(selection_label, QString::fromStdString(currentBranchName));
    system(setUpstream(currentBranchName).c_str());
    hlayout->insertWidget(1, selection_label);
    connect(this, &ButtonControl::clicked, [=] {
      QString branchName = InputDialog::getText("Enter Branch Name", this).trimmed();
      if (branchName.isEmpty()) return;
      // Check if the branch is already the current branch
      if (branchName == QString::fromStdString(currentBranchName)) {
        ConfirmationDialog::alert("You are already using the branch\n" + QString::fromStdString(currentBranchName), this);
      } else { confirmAndChangeBranch(branchName.toStdString()); }
    });
  }

private:
  std::string setUpstream(const std::string& branch) {
    return "git config remote.origin.fetch '+refs/heads/" + branch + ":refs/remotes/origin/" + branch + "';"
           "git branch -u origin/" + branch + ";"
           "git config branch." + branch + ".merge refs/heads/" + branch;
  }

  void confirmAndChangeBranch(const std::string& branchName) {
    if (ConfirmationDialog::confirm("Are you sure to Change Branch?\nAny unsaved changes will be lost.\n\nReboot required, please wait.", this)) {
      QString errorMessage = "Branch " + QString::fromStdString(branchName) + " not found.\n\nPlease make sure the branch name is correct and the device is connected to the Internet.";
      std::string changeBranchCommand =
        "git branch -D " + branchName + "; "
        + "git fetch origin " + branchName + ":" + branchName + " || exit 1; "
        + "git checkout " + branchName + " --force && "
        + setUpstream(branchName) + " && reboot";
      if (system(changeBranchCommand.c_str()) != 0) ConfirmationDialog::alert(errorMessage, this);
    }
  }

private:
  QLabel *selection_label;
};

class SoftwarePanel : public ListWidget {
  Q_OBJECT
public:
  explicit SoftwarePanel(QWidget* parent = nullptr);
private:
  void showEvent(QShowEvent *event) override;
  void hideEvent(QHideEvent *event) override;
  void updateLabels();

  LabelControl *gitCommitLbl;
  LabelControl *osVersionLbl;
  LabelControl *versionLbl;
  LabelControl *lastUpdateLbl;
  ButtonControl *updateBtn;
  FixFingerprintSelect *fingerprintInput;
  ChangeBranchSelect *branchInput;
  FeaturesControl *featuresInput;

  Params params;
  QFileSystemWatcher *fs_watch;
  QTimer *timer;
};

class C2NetworkPanel: public ListWidget {
  Q_OBJECT
public:
  explicit C2NetworkPanel(QWidget* parent = nullptr);
private:
  void showEvent(QShowEvent *event) override;
  void hideEvent(QHideEvent *event) override;
  void updateLabels();
  QString getIPAddress();
  QString getNetworkType();
  LabelControl *ipaddress;
  LabelControl *networkType;
  QTimer *timer;
};

