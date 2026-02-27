#pragma once

#include <QLabel>
#include <QProgressBar>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

class AnimatedCounter;
class MissionCalendarWidget;
class RevenueChartWidget;

class DashboardPage : public QWidget {
  Q_OBJECT

public:
  explicit DashboardPage(QWidget *parent = nullptr);

public slots:
  void refresh();

signals:
  void quickActionClicked(int pageIndex);

private:
  void setupUi();
  QWidget *createStatCard(const QString &icon, const QString &value,
                          const QString &label, const QString &color,
                          int targetPage = -1);
  QWidget *createSectionHeader(const QString &icon, const QString &title);

  QLabel *m_welcomeLabel = nullptr;
  QLabel *m_dateLabel = nullptr;
  AnimatedCounter *m_clientCount = nullptr;
  AnimatedCounter *m_missionCount = nullptr;
  AnimatedCounter *m_invoiceCount = nullptr;
  AnimatedCounter *m_devisCount = nullptr;
  AnimatedCounter *m_revenue = nullptr;
  QLabel *m_clientTrend = nullptr;
  QLabel *m_missionTrend = nullptr;
  QLabel *m_invoiceTrend = nullptr;
  QLabel *m_devisTrend = nullptr;
  QLabel *m_revenueTrend = nullptr;
  QWidget *m_upcomingList = nullptr;
  QWidget *m_activityList = nullptr;
  QWidget *m_topClientsList = nullptr;
  RevenueChartWidget *m_chart = nullptr;
  MissionCalendarWidget *m_calendar = nullptr;

  // KPI widgets
  QLabel *m_conversionRate = nullptr;
  QProgressBar *m_conversionBar = nullptr;
  QLabel *m_overdueCount = nullptr;
  QLabel *m_pendingAmount = nullptr;
  QLabel *m_missionsDone = nullptr;

  QTimer *m_refreshTimer = nullptr;
};
