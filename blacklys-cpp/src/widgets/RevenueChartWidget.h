#pragma once

#include <QWidget>

class RevenueChartWidget : public QWidget {
  Q_OBJECT
public:
  explicit RevenueChartWidget(QWidget *parent = nullptr);

  void refreshData();

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  struct MonthData {
    QString label;
    double revenue = 0.0;
  };

  QList<MonthData> m_months;
  double m_maxRevenue = 0.0;
};
