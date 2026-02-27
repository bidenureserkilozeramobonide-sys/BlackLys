#pragma once

#include <QComboBox>
#include <QDate>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QTableWidget>
#include <QWidget>

class MissionCalendarWidget;
class PageHeader;

class MissionsPage : public QWidget {
  Q_OBJECT

public:
  explicit MissionsPage(QWidget *parent = nullptr);

public slots:
  void refresh();

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
  void onAddMission();
  void onEditMission(int row);
  void onViewMission(int row);
  void onDuplicateMission(int row);
  void onCalendarDayClicked(QDate date);
  void onExportCsv();
  void onExportPdf(int missionId);
  void onBatchDelete();
  void onFilterChanged();
  void onSearch(const QString &text);
  void onResetDateFilter();
  void onTableContextMenu(const QPoint &pos);
  void onQuickStatusChange(int row, const QString &newStatus);

private:
  void setupUi();
  void setupShortcuts();
  void populateTable();
  void updateStats();
  void updateHeaderSubtitle();
  QWidget *createStatCard(const QString &icon, const QString &value,
                          const QString &label, const QString &color);

  PageHeader *m_header = nullptr;
  QScrollArea *m_scrollArea = nullptr;
  QTableWidget *m_table = nullptr;
  QLineEdit *m_searchInput = nullptr;
  QComboBox *m_filterCombo = nullptr;
  MissionCalendarWidget *m_calendar = nullptr;
  QDate m_selectedDate;

  // Stat card value labels (updated on refresh)
  QLabel *m_statPlanned = nullptr;
  QLabel *m_statInProgress = nullptr;
  QLabel *m_statDone = nullptr;
  QLabel *m_statCancelled = nullptr;

  // Stat card widgets (for event filter)
  QWidget *m_cardPlanned = nullptr;
  QWidget *m_cardInProgress = nullptr;
  QWidget *m_cardDone = nullptr;
  QWidget *m_cardCancelled = nullptr;

  // Date filter indicator
  QWidget *m_dateFilterRow = nullptr;
  QLabel *m_dateFilterLabel = nullptr;

  // Empty state
  QWidget *m_emptyState = nullptr;
};
