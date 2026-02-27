#pragma once

#include <QLabel>
#include <QTimeLine>

class AnimatedCounter : public QLabel {
  Q_OBJECT

public:
  explicit AnimatedCounter(QWidget *parent = nullptr);

  void setTargetValue(int value);
  void setTargetCurrency(double value);

private slots:
  void onFrame(int frame);

private:
  QTimeLine *m_timeline = nullptr;
  int m_targetInt = 0;
  double m_targetDouble = 0.0;
  bool m_isCurrency = false;
};
