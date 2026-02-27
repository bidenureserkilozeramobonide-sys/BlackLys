#pragma once

#include <QLabel>
#include <QPropertyAnimation>
#include <QTimer>
#include <QWidget>

class ToastWidget : public QWidget {
  Q_OBJECT
  Q_PROPERTY(int yOffset READ yOffset WRITE setYOffset)

public:
  enum Type { Success, Error, Info };

  static void show(QWidget *parent, const QString &message, Type type = Success,
                   int durationMs = 3000);

  int yOffset() const { return m_yOffset; }
  void setYOffset(int y);

private:
  explicit ToastWidget(QWidget *parent, const QString &message, Type type,
                       int durationMs);

  QLabel *m_label = nullptr;
  QPropertyAnimation *m_animIn = nullptr;
  QPropertyAnimation *m_animOut = nullptr;
  int m_yOffset = -60;
};
