#pragma once

#include <QColor>
#include <QPoint>
#include <QWidget>

class ColorWheelWidget : public QWidget {
  Q_OBJECT
public:
  explicit ColorWheelWidget(const QString &title, QWidget *parent = nullptr);

  // Set normalized values
  void setColor(double hue, double sat); // hue: 0-360, sat: 0-1
  void reset() { setColor(0, 0); }

  double hue() const { return m_hue; }
  double sat() const { return m_sat; }

signals:
  void colorChanged(double hue, double sat);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  void updateFromPos(const QPoint &pos);
  QPointF posFromColor() const;

  QString m_title;
  double m_hue = 0.0; // 0-360
  double m_sat = 0.0; // 0-1
  bool m_dragging = false;
  QImage m_wheelCache;
  QRectF m_wheelRect;
};
