#include "selfdrive/ui/qt/window.h"

#include <QFontDatabase>
#include <QKeyEvent>

#include "selfdrive/hardware/hw.h"

MainWindow::MainWindow(QWidget *parent) : QWidget(parent) {
  // load fonts first so that any child widgets and styles can resolve them immediately
  QFontDatabase::addApplicationFont("../assets/fonts/Inter-Regular.ttf");
  QFontDatabase::addApplicationFont("../assets/fonts/Inter-Medium.ttf");
  QFontDatabase::addApplicationFont("../assets/fonts/Inter-SemiBold.ttf");
  QFontDatabase::addApplicationFont("../assets/fonts/Inter-Bold.ttf");
  QFontDatabase::addApplicationFont("../assets/fonts/Inter-ExtraBold.ttf");
  QFontDatabase::addApplicationFont("../assets/fonts/GlacialIndifference-Regular.otf");
  QFontDatabase::addApplicationFont("../assets/fonts/GlacialIndifference-Bold.otf");

  // global application styling and dark palette
  QPalette darkPalette;
  darkPalette.setColor(QPalette::WindowText, Qt::white);
  darkPalette.setColor(QPalette::Text, Qt::white);
  darkPalette.setColor(QPalette::ButtonText, Qt::white);
  qApp->setPalette(darkPalette);

  // no outline to prevent the focus rectangle
  setStyleSheet(R"(
    * {
      font-family: Inter, Glacial Indifference, sans-serif;
      color: #FFFFFF;
      outline: none;
    }
  )");
  setAttribute(Qt::WA_NoSystemBackground);

  main_layout = new QStackedLayout(this);
  main_layout->setMargin(0);

  homeWindow = new HomeWindow(this);
  main_layout->addWidget(homeWindow);
  QObject::connect(homeWindow, &HomeWindow::openSettings, this, &MainWindow::openSettings);
  QObject::connect(homeWindow, &HomeWindow::closeSettings, this, &MainWindow::closeSettings);
  connect(homeWindow, &HomeWindow::openTerms, [=] {
    onboardingWindow->showTerms();
    main_layout->setCurrentWidget(onboardingWindow);
  });

  connect(homeWindow, &HomeWindow::openTraining, [=] {
    onboardingWindow->showTrainingGuide();
    main_layout->setCurrentWidget(onboardingWindow);
  });

  settingsWindow = new SettingsWindow(this);
  main_layout->addWidget(settingsWindow);
  QObject::connect(settingsWindow, &SettingsWindow::closeSettings, this, &MainWindow::closeSettings);
  QObject::connect(settingsWindow, &SettingsWindow::showDriverView, [=] {
    homeWindow->showDriverView(true);
  });

  onboardingWindow = new OnboardingWindow(this);
  main_layout->addWidget(onboardingWindow);
  QObject::connect(onboardingWindow, &OnboardingWindow::onboardingDone, [=]() {
    main_layout->setCurrentWidget(homeWindow);
  });
  if (!onboardingWindow->completed()) {
    main_layout->setCurrentWidget(onboardingWindow);
  }

  QObject::connect(uiState(), &UIState::offroadTransition, [=](bool offroad) {
    if (!offroad) {
      closeSettings();
    }
  });
  QObject::connect(&device, &Device::interactiveTimout, [=]() {
    if (main_layout->currentWidget() == settingsWindow) {
      closeSettings();
    }
  });

  if (getenv("UI_TEST_SETTINGS") != nullptr) {
    openSettings(0);
  }

  if (getenv("UI_TEST_SCREENSHOT") != nullptr) {
    QTimer::singleShot(1500, [=]() {
      QPixmap pix = this->grab();
      pix.save(getenv("UI_TEST_SCREENSHOT"));
      qApp->quit();
    });
  }
}

void MainWindow::openSettings(int panel_index) {
  if (settingsWindow) {
    settingsWindow->setCurrentPanel(panel_index);
  }
  main_layout->setCurrentWidget(settingsWindow);
}

void MainWindow::closeSettings() {
  main_layout->setCurrentWidget(homeWindow);

  if (uiState()->scene.started) {
    homeWindow->showSidebar(false);
  }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::KeyPress) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
    if (keyEvent->key() == Qt::Key_O) {
      bool onroad = !uiState()->scene.started;
      if (onroad) {
        setenv("FORCE_ONROAD", "1", 1);
      } else {
        unsetenv("FORCE_ONROAD");
      }
      uiState()->scene.started = onroad;
      emit uiState()->offroadTransition(!onroad);
      return true;
    }
  }

  const static QSet<QEvent::Type> evts({QEvent::MouseButtonPress, QEvent::MouseMove,
                                 QEvent::TouchBegin, QEvent::TouchUpdate, QEvent::TouchEnd});

  if (evts.contains(event->type())) {
    device.resetInteractiveTimout();
#ifdef QCOM
    // filter out touches while in android activity
    if (HardwareEon::launched_activity) {
      HardwareEon::check_activity();
      if (HardwareEon::launched_activity) {
        return true;
      }
    }
#endif
  }
  return false;
}
