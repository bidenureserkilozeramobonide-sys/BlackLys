#include "HslWheelWidget.h"
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QtMath>

HslWheelWidget::HslWheelWidget(QWidget *parent) : QWidget(parent) {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(4);

  // Reserve space for the wheel (painted in paintEvent — NO child widgets here)
  mainLayout->addSpacing(150);

  // Create color buttons (positioned absolutely in resizeEvent)
  for (int i = 0; i < kCount; ++i) {
    m_colorBtns[i] = new QPushButton(this);
    m_colorBtns[i]->setFixedSize(20, 14);
    m_colorBtns[i]->setCursor(Qt::PointingHandCursor);
    m_colorBtns[i]->setToolTip(kNames[i]);

    QColor c = QColor::fromHsv(kBaseHues[i], 220, 200);
    m_colorBtns[i]->setStyleSheet(
        QString(
            "QPushButton { background: %1; border: 1px solid rgba(0,0,0,0.3);"
            " border-radius: 3px; }"
            "QPushButton:hover { border: 1px solid rgba(255,255,255,0.5); }")
            .arg(c.name()));

    int idx = i;
    connect(m_colorBtns[i], &QPushButton::clicked, this,
            [this, idx]() { selectColor(idx); });
  }

  // Slider rows
  auto makeRow = [&](const QString &label, QSlider *&slider, QLabel *&valLbl) {
    auto *row = new QWidget(this);
    auto *rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 2, 0, 2);
    rl->setSpacing(6);

    auto *lbl = new QLabel(label, row);
    lbl->setFixedWidth(18);
    lbl->setStyleSheet(
        "color: #a1a1aa; font-size: 11px; background: transparent;");
    rl->addWidget(lbl);

    slider = new QSlider(Qt::Horizontal, row);
    slider->setRange(-100, 100);
    slider->setValue(0);
    slider->setFixedHeight(20);
    slider->setFocusPolicy(Qt::NoFocus);
    rl->addWidget(slider, 1);

    valLbl = new QLabel("0", row);
    valLbl->setFixedWidth(30);
    valLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    valLbl->setStyleSheet("color: #71717a; font-size: 10px; font-family: "
                          "'Consolas'; background: transparent;");
    rl->addWidget(valLbl);

    mainLayout->addWidget(row);
  };

  makeRow("H", m_hueSlider, m_hueValLabel);
  makeRow("S", m_satSlider, m_satValLabel);
  makeRow("L", m_lumSlider, m_lumValLabel);

  connect(m_hueSlider, &QSlider::valueChanged, this,
          &HslWheelWidget::onSliderMoved);
  connect(m_satSlider, &QSlider::valueChanged, this,
          &HslWheelWidget::onSliderMoved);
  connect(m_lumSlider, &QSlider::valueChanged, this,
          &HslWheelWidget::onSliderMoved);

  m_colorLabel = nullptr;
  setCursor(Qt::ArrowCursor);
  setMinimumHeight(220);
  selectColor(0);
}

void HslWheelWidget::setValues(int i, double h, double s, double l) {
  if (i < 0 || i >= kCount)
    return;
  m_hue[i] = h;
  m_sat[i] = s;
  m_lum[i] = l;
  if (i == m_selected)
    syncSlidersToSelected();
  update();
}

void HslWheelWidget::resetAll() {
  for (int i = 0; i < kCount; ++i) {
    m_hue[i] = 0;
    m_sat[i] = 0;
    m_lum[i] = 0;
  }
  syncSlidersToSelected();
  update();
}

void HslWheelWidget::selectColor(int idx) {
  if (idx < 0 || idx >= kCount)
    return;
  m_selected = idx;
  updateButtonStyles();
  updateSliderGradients();
  syncSlidersToSelected();
  update();
}

void HslWheelWidget::updateButtonStyles() {
  for (int i = 0; i < kCount; ++i) {
    QColor c = QColor::fromHsv(kBaseHues[i], 220, 200);
    if (i == m_selected) {
      m_colorBtns[i]->setStyleSheet(
          QString("QPushButton { background: %1; border: 2px solid #ffffff;"
                  " border-radius: 3px; }")
              .arg(c.name()));
    } else {
      m_colorBtns[i]->setStyleSheet(
          QString(
              "QPushButton { background: %1; border: 1px solid rgba(0,0,0,0.3);"
              " border-radius: 3px; }"
              "QPushButton:hover { border: 1px solid rgba(255,255,255,0.5); }")
              .arg(c.name()));
    }
  }
}

void HslWheelWidget::updateSliderGradients() {
  int baseHue = kBaseHues[m_selected];
  QColor base = QColor::fromHsv(baseHue, 220, 200);
  QString baseName = base.name();

  // H slider: hue shifted left → base hue → hue shifted right
  int hLeft = (baseHue - 30 + 360) % 360;
  int hRight = (baseHue + 30) % 360;
  QColor cLeft = QColor::fromHsv(hLeft, 200, 200);
  QColor cRight = QColor::fromHsv(hRight, 200, 200);
  m_hueSlider->setStyleSheet(
      QString("QSlider::groove:horizontal {"
              "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
              "    stop:0 %1, stop:0.5 %2, stop:1 %3);"
              "  height: 4px; border-radius: 2px; }"
              "QSlider::sub-page:horizontal { background: none; }"
              "QSlider::add-page:horizontal { background: none; }"
              "QSlider::handle:horizontal { background: #e4e4e7;"
              " border: 1px solid #27272a; width: 10px; height: 10px;"
              " margin: -3px 0; border-radius: 5px; }")
          .arg(cLeft.name(), baseName, cRight.name()));

  // S slider: desaturated → saturated
  QColor desat = QColor::fromHsv(baseHue, 40, 180);
  QColor sat = QColor::fromHsv(baseHue, 255, 220);
  m_satSlider->setStyleSheet(
      QString("QSlider::groove:horizontal {"
              "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
              "    stop:0 %1, stop:1 %2);"
              "  height: 4px; border-radius: 2px; }"
              "QSlider::sub-page:horizontal { background: none; }"
              "QSlider::add-page:horizontal { background: none; }"
              "QSlider::handle:horizontal { background: #e4e4e7;"
              " border: 1px solid #27272a; width: 10px; height: 10px;"
              " margin: -3px 0; border-radius: 5px; }")
          .arg(desat.name(), sat.name()));

  // L slider: dark → base color → bright
  QColor dark = QColor::fromHsv(baseHue, 200, 40);
  QColor bright = QColor::fromHsv(baseHue, 60, 255);
  m_lumSlider->setStyleSheet(
      QString("QSlider::groove:horizontal {"
              "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
              "    stop:0 %1, stop:0.5 %2, stop:1 %3);"
              "  height: 4px; border-radius: 2px; }"
              "QSlider::sub-page:horizontal { background: none; }"
              "QSlider::add-page:horizontal { background: none; }"
              "QSlider::handle:horizontal { background: #e4e4e7;"
              " border: 1px solid #27272a; width: 10px; height: 10px;"
              " margin: -3px 0; border-radius: 5px; }")
          .arg(dark.name(), baseName, bright.name()));
}

void HslWheelWidget::syncSlidersToSelected() {
  m_hueSlider->blockSignals(true);
  m_satSlider->blockSignals(true);
  m_lumSlider->blockSignals(true);

  m_hueSlider->setValue(qRound(m_hue[m_selected] / 0.3));
  m_satSlider->setValue(qRound(m_sat[m_selected] * 100));
  m_lumSlider->setValue(qRound(m_lum[m_selected] * 100));

  m_hueValLabel->setText(QString::number(m_hue[m_selected], 'f', 0));
  m_satValLabel->setText(QString::number(m_sat[m_selected] * 100, 'f', 0));
  m_lumValLabel->setText(QString::number(m_lum[m_selected] * 100, 'f', 0));

  m_hueSlider->blockSignals(false);
  m_satSlider->blockSignals(false);
  m_lumSlider->blockSignals(false);
}

void HslWheelWidget::onSliderMoved() {
  m_hue[m_selected] = m_hueSlider->value() * 0.3;
  m_sat[m_selected] = m_satSlider->value() * 0.01;
  m_lum[m_selected] = m_lumSlider->value() * 0.01;

  m_hueValLabel->setText(QString::number(m_hue[m_selected], 'f', 0));
  m_satValLabel->setText(QString::number(m_sat[m_selected] * 100, 'f', 0));
  m_lumValLabel->setText(QString::number(m_lum[m_selected] * 100, 'f', 0));

  update();
  emit valueChanged();
}

void HslWheelWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  rebuildCache();

  // Position color buttons on the right side of the wheel area
  int btnX = width() - 24;
  int btnY = 6;
  for (int i = 0; i < kCount; ++i) {
    m_colorBtns[i]->move(btnX, btnY);
    btnY += 17;
  }
}

void HslWheelWidget::rebuildCache() {
  // Leave 30px on the right for buttons
  int available = width() - 30;
  int wheelSize = qMin(available, 140);
  if (wheelSize <= 0)
    return;

  int xOff = (available - wheelSize) / 2;
  int yOff = 4;
  m_wheelRect = QRectF(xOff, yOff, wheelSize, wheelSize);

  qreal dpr = devicePixelRatioF();
  if (dpr < 1.0)
    dpr = 1.0;
  int cs = qCeil(wheelSize * dpr);

  m_wheelCache = QImage(cs, cs, QImage::Format_ARGB32);
  m_wheelCache.setDevicePixelRatio(dpr);
  m_wheelCache.fill(Qt::transparent);

  double radius = cs / 2.0;
  double cxy = cs / 2.0;
  double innerRadius = radius * 0.62;

  for (int y = 0; y < cs; ++y) {
    QRgb *scanLine = reinterpret_cast<QRgb *>(m_wheelCache.scanLine(y));
    for (int x = 0; x < cs; ++x) {
      double dx = x + 0.5 - cxy;
      double dy = cxy - (y + 0.5);
      double dist = qSqrt(dx * dx + dy * dy);

      if (dist >= innerRadius && dist <= radius) {
        double angleRad = qAtan2(dy, dx);
        double hue = angleRad * 180.0 / M_PI;
        if (hue < 0)
          hue += 360.0;

        double ringWidth = radius - innerRadius;
        double ringPos = (dist - innerRadius) / ringWidth;
        double sat = 0.5 + ringPos * 0.5;

        QColor col = QColor::fromHsvF(hue / 360.0, sat, 0.9);

        double alpha = 1.0;
        if (dist > radius - 1.0)
          alpha = radius - dist;
        if (dist < innerRadius + 1.0)
          alpha = qMin(alpha, dist - innerRadius);
        alpha = qBound(0.0, alpha, 1.0);
        col.setAlphaF(alpha);

        scanLine[x] = col.rgba();
      } else {
        scanLine[x] = 0;
      }
    }
  }
}

QPointF HslWheelWidget::indicatorPos() const {
  double cx = m_wheelRect.center().x();
  double cy = m_wheelRect.center().y();
  double radius = m_wheelRect.width() / 2.0;
  double midRing = radius * 0.81;
  double angle = kBaseHues[m_selected] * M_PI / 180.0;
  return QPointF(cx + midRing * qCos(angle), cy - midRing * qSin(angle));
}

int HslWheelWidget::nearestColor(double angleDeg) const {
  int bestIdx = 0;
  double bestDist = 999;
  for (int i = 0; i < kCount; ++i) {
    double d = qAbs(angleDeg - kBaseHues[i]);
    if (d > 180)
      d = 360 - d;
    if (d < bestDist) {
      bestDist = d;
      bestIdx = i;
    }
  }
  return bestIdx;
}

void HslWheelWidget::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  // Draw cached hue ring
  if (!m_wheelCache.isNull()) {
    p.drawImage(m_wheelRect.topLeft(), m_wheelCache);
  }

  // Center fill
  double cx = m_wheelRect.center().x();
  double cy = m_wheelRect.center().y();
  double innerR = m_wheelRect.width() / 2.0 * 0.57;
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(24, 24, 27, 200));
  p.drawEllipse(QPointF(cx, cy), innerR, innerR);

  // Draggable indicator
  QPointF pos = indicatorPos();
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(0, 0, 0, 100));
  p.drawEllipse(pos, 7, 7);
  p.setPen(QPen(QColor(255, 255, 255), 2.0));
  p.setBrush(Qt::NoBrush);
  p.drawEllipse(pos, 5, 5);
}

void HslWheelWidget::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton)
    return;

  QPointF pos = event->pos();
  QPointF c = m_wheelRect.center();
  double dx = pos.x() - c.x();
  double dy = c.y() - pos.y();
  double dist = qSqrt(dx * dx + dy * dy);
  double r = m_wheelRect.width() / 2.0;

  if (dist > r * 0.3 && dist < r * 1.15) {
    m_dragging = true;
    setCursor(Qt::ClosedHandCursor);
    double angle = qAtan2(dy, dx) * 180.0 / M_PI;
    if (angle < 0)
      angle += 360.0;
    selectColor(nearestColor(angle));
  }
}

void HslWheelWidget::mouseMoveEvent(QMouseEvent *event) {
  if (!m_dragging)
    return;

  QPointF c = m_wheelRect.center();
  double dx = event->pos().x() - c.x();
  double dy = c.y() - event->pos().y();
  double angle = qAtan2(dy, dx) * 180.0 / M_PI;
  if (angle < 0)
    angle += 360.0;

  int idx = nearestColor(angle);
  if (idx != m_selected)
    selectColor(idx);
}

void HslWheelWidget::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton && m_dragging) {
    m_dragging = false;
    setCursor(Qt::ArrowCursor);
  }
}
