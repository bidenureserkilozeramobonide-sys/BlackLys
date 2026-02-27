#include "AnimatedCounter.h"

AnimatedCounter::AnimatedCounter(QWidget *parent) : QLabel(parent) {
  m_timeline = new QTimeLine(600, this);
  m_timeline->setFrameRange(0, 100);
  m_timeline->setEasingCurve(QEasingCurve::OutCubic);
  connect(m_timeline, &QTimeLine::frameChanged, this,
          &AnimatedCounter::onFrame);
}

void AnimatedCounter::setTargetValue(int value) {
  m_isCurrency = false;
  m_targetInt = value;
  m_timeline->stop();
  m_timeline->start();
}

void AnimatedCounter::setTargetCurrency(double value) {
  m_isCurrency = true;
  m_targetDouble = value;
  m_timeline->stop();
  m_timeline->start();
}

void AnimatedCounter::onFrame(int frame) {
  double pct = frame / 100.0;
  if (m_isCurrency) {
    double cur = m_targetDouble * pct;
    setText(QString("%1 EUR").arg(cur, 0, 'f', 2));
  } else {
    int cur = static_cast<int>(m_targetInt * pct);
    setText(QString::number(cur));
  }
}
