#include "ColorWheelWidget.h"
#include <QConicalGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QtMath>

ColorWheelWidget::ColorWheelWidget(const QString &title, QWidget *parent)
    : QWidget(parent), m_title(title) {
  setMinimumSize(90, 120);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  setCursor(Qt::CrossCursor);
}

void ColorWheelWidget::setColor(double hue, double sat) {
  if (qFuzzyCompare(m_hue, hue) && qFuzzyCompare(m_sat, sat))
    return;
  m_hue = hue;
  m_sat = qBound(0.0, sat, 1.0);
  update();
}

void ColorWheelWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);

  int titleHeight = 22;
  int size = qMin(width(), height() - titleHeight) - 8;
  if (size <= 0)
    return;

  m_wheelRect = QRectF((width() - size) / 2.0, titleHeight + 4, size, size);

  qreal dpr = devicePixelRatioF();
  if (dpr < 1.0)
    dpr = 1.0;
  int cacheSize = qCeil(size * dpr);

  // High-quality pixel-by-pixel generation
  m_wheelCache = QImage(cacheSize, cacheSize, QImage::Format_ARGB32);
  m_wheelCache.setDevicePixelRatio(dpr);
  m_wheelCache.fill(Qt::transparent);

  double radius = cacheSize / 2.0;
  double cxy = cacheSize / 2.0;

  for (int y = 0; y < cacheSize; ++y) {
    QRgb *scanLine = reinterpret_cast<QRgb *>(m_wheelCache.scanLine(y));
    for (int x = 0; x < cacheSize; ++x) {
      double dx = x + 0.5 - cxy;
      double dy = cxy - (y + 0.5); // Y point up standard math
      double dist = qSqrt(dx * dx + dy * dy);

      if (dist <= radius) {
        double sat = dist / radius;
        double angleRad = qAtan2(dy, dx);
        double hue = angleRad * 180.0 / M_PI;
        if (hue < 0)
          hue += 360.0;

        QColor col = QColor::fromHsvF(hue / 360.0, sat, 1.0);

        // Anti-alias outer edge
        double alpha = 1.0;
        if (dist > radius - 1.0) {
          alpha = radius - dist;
        }
        col.setAlphaF(alpha);

        scanLine[x] = col.rgba();
      } else {
        scanLine[x] = 0; // Transparent
      }
    }
  }
}

void ColorWheelWidget::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  // Draw title
  p.setPen(QColor("#a1a1aa"));
  QFont font = p.font();
  font.setPixelSize(11);
  font.setBold(true);
  p.setFont(font);
  p.drawText(QRect(0, 0, width(), 20), Qt::AlignCenter, m_title);

  // Draw cached wheel (centered pixel-perfect)
  if (!m_wheelCache.isNull()) {
    p.drawImage(m_wheelRect.topLeft(), m_wheelCache);
  }

  // Soft border ring without hard anti-aliasing artifacts
  p.setPen(QPen(QColor(255, 255, 255, 20), 1.0));
  p.setBrush(Qt::NoBrush);
  p.drawEllipse(m_wheelRect.adjusted(0.5, 0.5, -0.5, -0.5));

  // Modern indicator
  QPointF picker = posFromColor();

  // Shadow/outer ring
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(0, 0, 0, 150));
  p.drawEllipse(picker, 6, 6);

  // Handle fill edge
  p.setPen(QPen(QColor(255, 255, 255), 2.0));
  p.setBrush(Qt::NoBrush);
  p.drawEllipse(picker, 4, 4);
}

QPointF ColorWheelWidget::posFromColor() const {
  double radius = m_wheelRect.width() / 2.0;
  double r = m_sat * radius;
  double angleRad = m_hue * M_PI / 180.0;
  // X = center + r*cos, Y = center - r*sin (Qt Y is down, standard math Y is
  // up)
  double x = m_wheelRect.center().x() + r * qCos(angleRad);
  double y = m_wheelRect.center().y() - r * qSin(angleRad);
  return QPointF(x, y);
}

void ColorWheelWidget::updateFromPos(const QPoint &pos) {
  QPointF center = m_wheelRect.center();
  double dx = pos.x() - center.x();
  double dy = center.y() - pos.y(); // invert Y

  double radius = m_wheelRect.width() / 2.0;
  double dist = qSqrt(dx * dx + dy * dy);

  m_sat = qBound(0.0, dist / radius, 1.0);

  if (dist > 0) {
    // atan2 gives -PI to PI
    double angleRad = qAtan2(dy, dx);
    double angleDeg = angleRad * 180.0 / M_PI;
    if (angleDeg < 0)
      angleDeg += 360.0;
    m_hue = angleDeg;
  }

  update();
  emit colorChanged(m_hue, m_sat);
}

void ColorWheelWidget::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton && m_wheelRect.contains(event->pos())) {
    m_dragging = true;
    updateFromPos(event->pos());
  }
}

void ColorWheelWidget::mouseMoveEvent(QMouseEvent *event) {
  if (m_dragging) {
    updateFromPos(event->pos());
  }
}

void ColorWheelWidget::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    m_dragging = false;
  }
}
