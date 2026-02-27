#pragma once

#include <QComboBox>
#include <QDateEdit>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QWidget>

class AnimatedCounter;

class InvoicesPage : public QWidget {
  Q_OBJECT

public:
  explicit InvoicesPage(QWidget *parent = nullptr);

public slots:
  void refresh();

private slots:
  void onAddInvoice();
  void onEditInvoice(int row);
  void onDeleteInvoice();
  void onFilterChanged();
  void onExportCsv();
  void onBatchDelete();
  void onDuplicateInvoice(int row);
  void onMarkAsPaid(int row);
  void onMarkAsSent(int row);
  void onSortByColumn(int column);

private:
  void setupUi();
  void populateTable();
  void refreshStats();
  void exportPdf(int invoiceId);
  QWidget *createStatCard(const QString &icon, const QString &value,
                          const QString &label, const QString &color);

  QTableWidget *m_table = nullptr;
  QComboBox *m_filterCombo = nullptr;
  QLineEdit *m_searchInput = nullptr;
  QScrollArea *m_scrollArea = nullptr;
  QDateEdit *m_dateFrom = nullptr;
  QDateEdit *m_dateTo = nullptr;

  // Stat card animated counters
  AnimatedCounter *m_statTotal = nullptr;
  AnimatedCounter *m_statPaid = nullptr;
  AnimatedCounter *m_statPending = nullptr;
  AnimatedCounter *m_statOverdue = nullptr;

  // Sorting
  int m_sortColumn = -1;
  bool m_sortAscending = true;
};
