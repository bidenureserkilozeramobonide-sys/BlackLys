#pragma once

#include "../database/MissionModel.h"
#include <QDate>
#include <QWidget>

class MissionCalendarWidget : public QWidget {
  Q_OBJECT
public:
  explicit MissionCalendarWidget(QWidget *parent = nullptr);

  void setMonth(int year, int month);
  void refreshData();
  void setSelectedDate(QDate date);

signals:
  void dayClicked(QDate date);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;

private:
  int m_year;
  int m_month;
  QList<Mission> m_missions;
  QDate m_selectedDate;

  // Geometry cache
  QRect cellRect(int row, int col) const;
  int m_headerHeight = 48;
  int m_dayHeaderHeight = 28;
  int m_cellPadding = 2;
};
