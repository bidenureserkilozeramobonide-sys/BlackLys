#pragma once

#include <QLineEdit>
#include <QWidget>

class SettingsPage : public QWidget {
  Q_OBJECT

public:
  explicit SettingsPage(QWidget *parent = nullptr);

public slots:
  void refresh();

private slots:
  void onSave();
  void onBackup();
  void onRestore();

private:
  void setupUi();
  void loadSettings();

  // Company info
  QLineEdit *m_companyName = nullptr;
  QLineEdit *m_companyAddress = nullptr;
  QLineEdit *m_companyPhone = nullptr;
  QLineEdit *m_companyEmail = nullptr;
  QLineEdit *m_companySiret = nullptr;

  // Defaults
  QLineEdit *m_defaultTva = nullptr;
  QLineEdit *m_invoicePrefix = nullptr;
  QLineEdit *m_outputDir = nullptr;
  QLineEdit *m_topazPath = nullptr;
};
