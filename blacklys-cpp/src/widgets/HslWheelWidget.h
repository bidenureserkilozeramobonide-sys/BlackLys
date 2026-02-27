#pragma once

#include <QImage>
#include <QLabel>
#include <QPushButton>
#include <QRectF>
#include <QSlider>
#include <QWidget>

// Interactive HSL Wheel: drag a cursor around a hue ring to select a color.
// Quick-access color buttons on the right side of the wheel.
// 3 sliders (H/S/L) adjust the selected color.
class HslWheelWidget : public QWidget {
  Q_OBJECT
public:
  explicit HslWheelWidget(QWidget *parent = nullptr);

  // Get current adjustments for the 8 colors
  double hslHue(int i) const { return m_hue[i]; }
  double hslSat(int i) const { return m_sat[i]; }
  double hslLum(int i) const { return m_lum[i]; }

  // Set values programmatically (e.g. from preset load)
  void setValues(int i, double h, double s, double l);
  void resetAll();

signals:
  void valueChanged(); // emitted whenever any H/S/L value changes

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  void rebuildCache();
  QPointF indicatorPos() const;
  int nearestColor(double angleDeg) const;

  // 8 hue channels
  static constexpr int kCount = 8;
  static constexpr int kBaseHues[8] = {0, 30, 60, 120, 180, 240, 270, 300};
  static constexpr const char *kNames[8] = {
      "Red", "Orange", "Yellow", "Green", "Aqua", "Blue", "Purple", "Magenta"};

  // Per-color adjustments
  double m_hue[8] = {}; // -30..+30
  double m_sat[8] = {}; // -1..+1
  double m_lum[8] = {}; // -1..+1

  int m_selected = 0;
  bool m_dragging = false;

  // Wheel rendering
  QImage m_wheelCache;
  QRectF m_wheelRect;

  // Quick-access color buttons
  QPushButton *m_colorBtns[8] = {};

  // Sliders below the wheel
  QSlider *m_hueSlider = nullptr;
  QSlider *m_satSlider = nullptr;
  QSlider *m_lumSlider = nullptr;
  QLabel *m_colorLabel = nullptr;
  QLabel *m_hueValLabel = nullptr;
  QLabel *m_satValLabel = nullptr;
  QLabel *m_lumValLabel = nullptr;

  void selectColor(int idx);
  void syncSlidersToSelected();
  void onSliderMoved();
  void updateButtonStyles();
  void updateSliderGradients();
};
