#include "ToastWidget.h"

#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>

ToastWidget::ToastWidget(QWidget *parent, const QString &message, Type type,
                         int durationMs)
    : QWidget(parent) {
  setFixedHeight(48);
  setAttribute(Qt::WA_DeleteOnClose);
  setWindowFlags(Qt::FramelessWindowHint | Qt::SubWindow);

  // Colors per type
  QString bg, border, fg, icon;
  switch (type) {
  case Success:
    bg = "rgba(16,185,129,0.15)";
    border = "rgba(16,185,129,0.4)";
    fg = "#6ee7b7";
    icon = QString(QChar(0x2713)) + "  "; // ✓
    break;
  case Error:
    bg = "rgba(239,68,68,0.15)";
    border = "rgba(239,68,68,0.4)";
    fg = "#fca5a5";
    icon = QString(QChar(0x2717)) + "  "; // ✗
    break;
  case Info:
    bg = "rgba(245,158,11,0.15)";
    border = "rgba(245,158,11,0.4)";
    fg = "#fcd34d";
    icon = QString(QChar(0x2139)) + "  "; // ℹ
    break;
  }

  setStyleSheet(QString("ToastWidget { background: %1; border: 1px solid %2; "
                        "border-radius: 10px; }")
                    .arg(bg, border));

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(20, 8, 20, 8);

  m_label = new QLabel(icon + message, this);
  m_label->setStyleSheet(
      QString("color: %1; font-size: 13px; font-weight: 600; "
              "background: transparent;")
          .arg(fg));
  layout->addWidget(m_label);

  // Size & position
  int w = qMin(parent->width() - 40, 420);
  setFixedWidth(w);
  int xPos = parent->width() - w - 20;
  move(xPos, m_yOffset);

  // Slide-in animation
  m_animIn = new QPropertyAnimation(this, "yOffset", this);
  m_animIn->setDuration(300);
  m_animIn->setStartValue(-60);
  m_animIn->setEndValue(16);
  m_animIn->setEasingCurve(QEasingCurve::OutCubic);

  // Slide-out animation
  m_animOut = new QPropertyAnimation(this, "yOffset", this);
  m_animOut->setDuration(250);
  m_animOut->setStartValue(16);
  m_animOut->setEndValue(-60);
  m_animOut->setEasingCurve(QEasingCurve::InCubic);
  connect(m_animOut, &QPropertyAnimation::finished, this, &QWidget::close);

  // Show & animate
  QWidget::show();
  raise();
  m_animIn->start();

  // Auto-dismiss
  QTimer::singleShot(durationMs, this, [this]() { m_animOut->start(); });
}

void ToastWidget::setYOffset(int y) {
  m_yOffset = y;
  int xPos = parentWidget()->width() - width() - 20;
  move(xPos, y);
}

void ToastWidget::show(QWidget *parent, const QString &message, Type type,
                       int durationMs) {
  new ToastWidget(parent, message, type, durationMs);
}
