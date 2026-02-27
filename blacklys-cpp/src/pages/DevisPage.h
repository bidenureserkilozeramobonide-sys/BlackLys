#pragma once

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QWidget>

class DevisPage : public QWidget {
  Q_OBJECT
public:
  explicit DevisPage(QWidget *parent = nullptr);

public slots:
  void refresh();

private slots:
  void onAddQuote();
  void onEditQuote(int row);
  void onDeleteQuote();
  void onConvertToInvoice(int quoteId);
  void onDuplicateQuote(int quoteId);
  void exportPdf(int quoteId);
  void onBatchDelete();
  void onExportCsv();
  void onFilterChanged();
  void onSortByColumn(int column);

private:
  void setupUi();
  void setupShortcuts();
  void populateTable();
  void checkExpiredQuotes();

  QTableWidget *m_table = nullptr;
  QComboBox *m_filterStatus = nullptr;
  QLineEdit *m_searchEdit = nullptr;
  QLabel *m_statsLabel = nullptr;

  int m_sortColumn = -1;
  Qt::SortOrder m_sortOrder = Qt::DescendingOrder;
  bool m_expiredChecked = false;
};
