#include "selfdrive/ui/qt/widgets/controls.h"

#include <QPainter>
#include <QStyleOption>
#include <QLineEdit>
#include <QDoubleSpinBox>

QFrame *horizontal_line(QWidget *parent) {
  QFrame *line = new QFrame(parent);
  line->setFrameShape(QFrame::StyledPanel);
  line->setStyleSheet(R"(
    margin-left: 40px;
    margin-right: 40px;
    border-width: 1px;
    border-bottom-style: solid;
    border-color: gray;
  )");
  line->setFixedHeight(2);
  return line;
}

AbstractControl::AbstractControl(const QString &title, const QString &desc, const QString &icon, QWidget *parent) : QFrame(parent) {
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

  QVBoxLayout *main_layout = new QVBoxLayout(this);
  main_layout->setMargin(0);

  hlayout = new QHBoxLayout;
  hlayout->setMargin(0);
  hlayout->setSpacing(20);

  // left icon
  if (!icon.isEmpty()) {
    QPixmap pix(icon);
    QLabel *icon_label = new QLabel();
    icon_label->setPixmap(pix.scaledToWidth(60, Qt::SmoothTransformation));
    icon_label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hlayout->addWidget(icon_label);
  }

  // title
  title_label = new QPushButton(title);
  title_label->setFixedHeight(90);
  title_label->setStyleSheet("font-size: 42px; font-weight: 500; text-align: left; border: none; background: transparent; color: #FFFFFF;");
  hlayout->addWidget(title_label);

  main_layout->addLayout(hlayout);

  // description
  if (!desc.isEmpty()) {
    description = new QLabel(desc);
    description->setContentsMargins(20, 16, 20, 16);
    description->setStyleSheet("font-size: 32px; color: #94A3B8;");
    description->setWordWrap(true);
    description->setVisible(false);
    main_layout->addWidget(description);

    connect(title_label, &QPushButton::clicked, [=]() {
      if (!description->isVisible()) {
        emit showDescription();
      }
      description->setVisible(!description->isVisible());
    });
  }
  main_layout->addStretch();
}

void AbstractControl::hideEvent(QHideEvent *e) {
  if(description != nullptr) {
    description->hide();
  }
}

// controls
SpinboxControl::SpinboxControl(const QString &param, const QString &title, const QString &desc, const QString &unit, double range[], bool reboot_req, QWidget *parent) : AbstractControl(title, desc, "", parent) {

  reboot_required = reboot_req;
  spinbox.setRange(range[0], range[1]);
  spinbox.setSingleStep(range[2]);
  spinbox.setDecimals(1);
  spinbox.setObjectName("spinbox");

  // set default value, must set after setRange
  if (params.get(param.toStdString()) == "") {
    spinbox.setValue(0);
  }
  else {
    spinbox.setValue(std::stod(params.get(param.toStdString())));
  }

  spinbox.setSuffix(unit);
  spinbox.setAlignment(Qt::AlignHCenter);

  spinbox.setStyleSheet(R"(
    QDoubleSpinBox#spinbox {
      color: lightgray;
      border-style: none;
      width: 24px;
    }

    QDoubleSpinBox:disabled#spinbox {
      color: gray;
    }

    QDoubleSpinBox::down-button#spinbox, QDoubleSpinBox::up-button#spinbox {
      subcontrol-origin: margin;
      background: #393939;
      width: 72px;
      height: 72px;
    }

    QDoubleSpinBox::down-button#spinbox {
      subcontrol-position: center left;
    }

    QDoubleSpinBox::up-button#spinbox {
      subcontrol-position: center right;
    }

    QDoubleSpinBox::down-button#spinbox,
    QDoubleSpinBox::up-button#spinbox {
      border-radius: 12px;
    }

    QDoubleSpinBox::down-button:pressed#spinbox,
    QDoubleSpinBox::up-button:pressed#spinbox {
      background-color: #4a4a4a;
    }

    QDoubleSpinBox::up-arrow#spinbox, QDoubleSpinBox::down-arrow#spinbox {
      subcontrol-origin: content;
      width: 48px;
      height: 48px;
    }

    QDoubleSpinBox::up-arrow#spinbox{
      image: url("/data/openpilot/selfdrive/assets/kommu/plus.png");
    }

    QDoubleSpinBox::down-arrow#spinbox{
      image: url("/data/openpilot/selfdrive/assets/kommu/minus.png");
    }
  )");

  spinbox.setFixedSize(400, 100);
  hlayout->addWidget(&spinbox);

  // remove the highlighting effect
  spinbox.findChildren<QLineEdit*> ().at(0)->setReadOnly(true);
  QObject::connect(&spinbox, SIGNAL(valueChanged(double)), this, SLOT(deselectTextEdit()), Qt::QueuedConnection);
  connect(spinbox.findChild<QLineEdit*> (), SIGNAL(cursorPositionChanged(int,int)), this, SLOT(deselectTextEdit()),Qt::QueuedConnection);

  // alert reboot required
  QObject::connect(&spinbox, SIGNAL(valueChanged(double)), this, SLOT(alertRebootRequired()), Qt::QueuedConnection);

  // set params
  key = param.toStdString();
  QObject::connect(&spinbox, SIGNAL(valueChanged(double)), this, SLOT(setParams(double)), Qt::QueuedConnection);
}

ButtonControl::ButtonControl(const QString &title, const QString &text, const QString &desc, bool no_style, QWidget *parent) : AbstractControl(title, desc, "", parent) {
  btn.setText(text);
  btn.setStyleSheet(R"(
    QPushButton {
      padding: 0 20px;
      border-radius: 28px;
      font-size: 30px;
      font-weight: 600;
      color: #FFFFFF;
      background-color: rgba(28, 36, 52, 0.85);
      border: 1.5px solid rgba(255, 255, 255, 0.16);
    }
    QPushButton:hover {
      border-color: rgba(0, 245, 212, 0.6);
      background-color: rgba(36, 48, 70, 0.9);
      color: #00F5D4;
    }
    QPushButton:pressed {
      border-color: #00F5D4;
      background-color: rgba(14, 22, 35, 0.95);
      color: #00F5D4;
    }
    QPushButton:disabled {
      color: rgba(255, 255, 255, 0.25);
      border-color: rgba(255, 255, 255, 0.06);
      background-color: rgba(20, 24, 34, 0.4);
    }
  )");

  if (no_style) {
    btn.setStyleSheet(R"(
      QPushButton {
        border: none;
        background: transparent;
        color: #E4E4E4;
        font-size: 32px;
        font-weight: 500;
      }
    )");
  }

  btn.setFixedSize(240, 84);
  QObject::connect(&btn, &QPushButton::clicked, this, &ButtonControl::clicked);
  hlayout->addWidget(&btn);
}

// SettingsCard implementation
SettingsCard::SettingsCard(const QString &title, QWidget *parent)
    : QFrame(parent), title_text(title) {
  setAttribute(Qt::WA_StyledBackground, false);
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

  outer_layout = new QVBoxLayout(this);
  outer_layout->setContentsMargins(36, 24, 36, 24);
  outer_layout->setSpacing(14);

  if (!title.isEmpty()) {
    title_label = new QLabel(title.toUpper());
    title_label->setStyleSheet(
        "font-size: 24px; font-weight: 700; color: #00F5D4; letter-spacing: 1.5px; border: none; background: transparent; padding-bottom: 4px;");
    outer_layout->addWidget(title_label);
  }

  inner_layout = new QVBoxLayout();
  inner_layout->setContentsMargins(0, 0, 0, 0);
  inner_layout->setSpacing(24);
  outer_layout->addLayout(inner_layout);
}

void SettingsCard::addItem(QWidget *w) {
  inner_layout->addWidget(w);
}

void SettingsCard::addItem(QLayout *layout) {
  inner_layout->addLayout(layout);
}

void SettingsCard::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHints(QPainter::Antialiasing);

  QRectF rc(1.5, 1.5, width() - 3.0, height() - 3.0);
  const float r = 24.0f;

  // Frosted obsidian glass card body
  p.setPen(QPen(QColor(255, 255, 255, 20), 1.5));
  p.setBrush(QColor(15, 20, 30, 230));
  p.drawRoundedRect(rc, r, r);

  // Specular top hairline reflection (cyan glow)
  QLinearGradient spec(rc.left() + 40, rc.top() + 1.5, rc.right() - 40, rc.top() + 1.5);
  spec.setColorAt(0.0, QColor(0, 245, 212, 0));
  spec.setColorAt(0.5, QColor(0, 245, 212, 70));
  spec.setColorAt(1.0, QColor(0, 245, 212, 0));

  p.setPen(QPen(spec, 1.8));
  p.drawLine(QPointF(rc.left() + 30, rc.top() + 1.5), QPointF(rc.right() - 30, rc.top() + 1.5));

  // Subtle dividers between items inside the card
  p.setPen(QPen(QColor(255, 255, 255, 16), 1.0));
  for (int i = 0; i < inner_layout->count() - 1; ++i) {
    auto item = inner_layout->itemAt(i);
    if (!item || !item->widget() || !item->widget()->isVisible()) continue;
    QRect r_item = item->geometry();
    int bottom = r_item.bottom() + inner_layout->spacing() / 2;
    p.drawLine(QPointF(rc.left() + 20, bottom), QPointF(rc.right() - 20, bottom));
  }
}

// ElidedLabel

ElidedLabel::ElidedLabel(QWidget *parent) : ElidedLabel({}, parent) {}

ElidedLabel::ElidedLabel(const QString &text, QWidget *parent) : QLabel(text.trimmed(), parent) {
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  setMinimumWidth(1);
  QFont f = font();
  f.setPixelSize(35);
  setFont(f);
  setStyleSheet("color: #E4E4E4; font-size: 35px;");
}

void ElidedLabel::resizeEvent(QResizeEvent* event) {
  QLabel::resizeEvent(event);
  lastText_ = elidedText_ = "";
}

void ElidedLabel::paintEvent(QPaintEvent *event) {
  const QString curText = text();
  if (curText != lastText_) {
    elidedText_ = fontMetrics().elidedText(curText, Qt::ElideRight, contentsRect().width());
    lastText_ = curText;
  }

  QPainter painter(this);
  painter.setFont(font());
  drawFrame(&painter);
  QStyleOption opt;
  opt.initFrom(this);
  opt.palette.setColor(QPalette::WindowText, QColor(228, 228, 228));
  opt.palette.setColor(QPalette::Text, QColor(228, 228, 228));
  style()->drawItemText(&painter, contentsRect(), alignment(), opt.palette, isEnabled(), elidedText_, foregroundRole());
}

ClickableWidget::ClickableWidget(QWidget *parent) : QWidget(parent) { }

void ClickableWidget::mouseReleaseEvent(QMouseEvent *event) {
  emit clicked();
}

// Fix stylesheets
void ClickableWidget::paintEvent(QPaintEvent *) {
  QStyleOption opt;
  opt.init(this);
  QPainter p(this);
  style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
