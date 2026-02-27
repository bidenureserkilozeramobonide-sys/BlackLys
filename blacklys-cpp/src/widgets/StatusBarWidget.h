#pragma once

#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>

class StatusBarWidget : public QWidget {
  Q_OBJECT

public:
  explicit StatusBarWidget(QWidget *parent = nullptr);

public slots:
  void refresh();

private:
  QLabel *m_stats = nullptr;
  QLabel *m_dbPath = nullptr;
};
