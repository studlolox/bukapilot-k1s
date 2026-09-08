#include "selfdrive/ui/qt/widgets/toggle.h"

#include <QPainter>

Toggle::Toggle(QWidget *parent) : QAbstractButton(parent),
_height(80),
_height_rect(60),
on(false),
_anim(new QPropertyAnimation(this, "offset_circle", this))
{
  _radius = _height / 2;
  _x_circle = _radius;
  _y_circle = _radius;
  _y_rect = (_height - _height_rect)/2;
  circleColor = QColor(0xffffff); // placeholder
  green = QColor(0xffffff); // placeholder
  setEnabled(true);
}

void Toggle::paintEvent(QPaintEvent *e) {
  this->setFixedHeight(_height);
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);

  QRect track_rc(0, _y_rect, width(), _height_rect);
  int track_r = _height_rect / 2;

  // Base inactive track
  p.setPen(QPen(QColor(255, 255, 255, 24), 1.5));
  p.setBrush(QColor(22, 28, 40));
  p.drawRoundedRect(track_rc, track_r, track_r);

  // Active cyan fill up to thumb circle
  if (_x_circle + _radius > track_r) {
    p.save();
    p.setClipRect(QRect(0, _y_rect - 4, _x_circle + _radius / 2, _height_rect + 8));
    p.setPen(Qt::NoPen);
    p.setBrush(green);
    p.drawRoundedRect(track_rc, track_r, track_r);

    // Active track subtle glow
    if (on) {
      p.setPen(QPen(QColor(0, 245, 212, 50), 4));
      p.setBrush(Qt::NoBrush);
      p.drawRoundedRect(track_rc, track_r, track_r);
    }
    p.restore();
  }

  // Thumb circle
  QRectF thumb_rc(_x_circle - _radius + 3, _y_circle - _radius + 3, 2 * _radius - 6, 2 * _radius - 6);
  // Thumb shadow
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(0, 0, 0, 80));
  p.drawEllipse(thumb_rc.translated(0, 2));

  // Thumb body
  QColor thumb_col = on ? QColor(255, 255, 255) : QColor(148, 163, 184);
  if (!enabled) thumb_col = QColor(100, 110, 125);
  p.setBrush(thumb_col);
  p.setPen(QPen(QColor(255, 255, 255, on ? 160 : 40), 1.0));
  p.drawEllipse(thumb_rc);
}

void Toggle::mouseReleaseEvent(QMouseEvent *e) {
  if (!enabled) {
    return;
  }
  const int left = _radius;
  const int right = width() - _radius;
  if ((_x_circle != left && _x_circle != right) || !this->rect().contains(e->localPos().toPoint())) {
    // If mouse release isn't in rect or animation is running, don't parse touch events
    return;
  }
  if (e->button() & Qt::LeftButton) {
    togglePosition();
    emit stateChanged(on);
  }
}

void Toggle::togglePosition() {
  on = !on;
  const int left = _radius;
  const int right = width() - _radius;
  _anim->setStartValue(on ? left + immediateOffset : right - immediateOffset);
  _anim->setEndValue(on ? right : left);
  _anim->setDuration(animation_duration);
  _anim->start();
  repaint();
}

void Toggle::enterEvent(QEvent *e) {
  QAbstractButton::enterEvent(e);
}

bool Toggle::getEnabled() {
  return enabled;
}

void Toggle::setEnabled(bool value) {
  enabled = value;
  if (value) {
    circleColor = QColor(255, 255, 255);
    green = QColor(0, 245, 212); // Luminous Cyan #00F5D4
  } else {
    circleColor = QColor(136, 136, 136);
    green = QColor(0, 140, 120);
  }
}
