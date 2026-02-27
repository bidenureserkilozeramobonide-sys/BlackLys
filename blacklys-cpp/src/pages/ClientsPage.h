#pragma once

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QWidget>

class ClientsPage : public QWidget {
  Q_OBJECT

public:
  explicit ClientsPage(QWidget *parent = nullptr);

public slots:
  void refresh();

private slots:
  void onSearch(const QString &text);
  void onAddClient();
  void onEditClient(int row);
  void onDeleteClient();
  void onViewClient(int row);
  void onExportCsv();
  void onImportCsv();
  void onBatchDelete();
  void onFilterChanged();

private:
  void setupUi();
  void populateTable(const QString &search = "");
  void updateStats();
  void refreshCompanyFilter();
  QColor avatarColor(const QString &name) const;
  QString initials(const QString &name) const;

  QTableWidget *m_table = nullptr;
  QLineEdit *m_searchInput = nullptr;
  QComboBox *m_companyFilter = nullptr;
  QLabel *m_totalBadge = nullptr;
  QLabel *m_monthBadge = nullptr;
  QLabel *m_companyBadge = nullptr;
  QLabel *m_resultCount = nullptr;
};
