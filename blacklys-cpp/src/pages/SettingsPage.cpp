#include "SettingsPage.h"
#include "widgets/PageHeader.h"

#include "database/Database.h"
#include "widgets/ToastWidget.h"

#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVBoxLayout>


// ════════════════════════════════════════════════
//  Settings helpers (key-value store)
// ════════════════════════════════════════════════

static QString getSetting(const QString &key,
                          const QString &defaultValue = "") {
  QSqlQuery q(Database::instance().db());
  q.prepare("SELECT value FROM settings WHERE key = ?");
  q.addBindValue(key);
  q.exec();
  if (q.next())
    return q.value(0).toString();
  return defaultValue;
}

static void setSetting(const QString &key, const QString &value) {
  QSqlQuery q(Database::instance().db());
  q.prepare("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)");
  q.addBindValue(key);
  q.addBindValue(value);
  q.exec();
}

// ════════════════════════════════════════════════
//  SettingsPage
// ════════════════════════════════════════════════

SettingsPage::SettingsPage(QWidget *parent) : QWidget(parent) {
  setObjectName("page");
  setupUi();
  loadSettings();
}

void SettingsPage::setupUi() {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(32, 24, 32, 24);
  mainLayout->setSpacing(16);

  auto *header =
      new PageHeader("Parametres", "Configuration de l'application", this);
  mainLayout->addWidget(header);

  // Scrollable content
  auto *scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  scroll->setStyleSheet(
      "QScrollArea { border: none; background: transparent; }");
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  auto *content = new QWidget(scroll);
  auto *contentLayout = new QVBoxLayout(content);
  contentLayout->setSpacing(20);

  // Style for groups
  QString groupStyle =
      "QGroupBox { border: 1px solid #27272a; border-radius: 10px; "
      "padding: 20px 16px 16px 16px; margin-top: 12px; "
      "background: #18181b; color: #e4e4e7; font-weight: 700; }"
      "QGroupBox::title { subcontrol-origin: margin; left: 16px; "
      "padding: 0 8px; color: #fbbf24; font-size: 13px; }";
  QString fieldStyle =
      "QLineEdit { background: #09090b; color: #e4e4e7; "
      "border: 1px solid #27272a; border-radius: 6px; padding: 8px; "
      "font-size: 13px; }"
      "QLineEdit:focus { border-color: #f59e0b; }";
  QString labelStyle = "QLabel { color: #a1a1aa; background: transparent; "
                       "font-size: 12px; }";

  // ────── Company Info ──────
  auto *companyGroup = new QGroupBox("Informations entreprise", content);
  companyGroup->setStyleSheet(groupStyle);
  auto *companyForm = new QFormLayout(companyGroup);
  companyForm->setSpacing(10);
  companyForm->setLabelAlignment(Qt::AlignRight);

  m_companyName = new QLineEdit(companyGroup);
  m_companyName->setPlaceholderText("BlackLys Studio");
  m_companyName->setStyleSheet(fieldStyle);
  auto *nameLabel = new QLabel("Raison sociale");
  nameLabel->setStyleSheet(labelStyle);
  companyForm->addRow(nameLabel, m_companyName);

  m_companyAddress = new QLineEdit(companyGroup);
  m_companyAddress->setPlaceholderText("123 Rue de la Photo, 75001 Paris");
  m_companyAddress->setStyleSheet(fieldStyle);
  auto *addrLabel = new QLabel("Adresse");
  addrLabel->setStyleSheet(labelStyle);
  companyForm->addRow(addrLabel, m_companyAddress);

  m_companyPhone = new QLineEdit(companyGroup);
  m_companyPhone->setPlaceholderText("+33 6 12 34 56 78");
  m_companyPhone->setStyleSheet(fieldStyle);
  auto *phoneLabel = new QLabel("Telephone");
  phoneLabel->setStyleSheet(labelStyle);
  companyForm->addRow(phoneLabel, m_companyPhone);

  m_companyEmail = new QLineEdit(companyGroup);
  m_companyEmail->setPlaceholderText("contact@blacklys.fr");
  m_companyEmail->setStyleSheet(fieldStyle);
  auto *emailLabel = new QLabel("Email");
  emailLabel->setStyleSheet(labelStyle);
  companyForm->addRow(emailLabel, m_companyEmail);

  m_companySiret = new QLineEdit(companyGroup);
  m_companySiret->setPlaceholderText("123 456 789 00012");
  m_companySiret->setStyleSheet(fieldStyle);
  auto *siretLabel = new QLabel("SIRET");
  siretLabel->setStyleSheet(labelStyle);
  companyForm->addRow(siretLabel, m_companySiret);

  contentLayout->addWidget(companyGroup);

  // ────── Defaults ──────
  auto *defaultsGroup = new QGroupBox("Parametres par defaut", content);
  defaultsGroup->setStyleSheet(groupStyle);
  auto *defaultsForm = new QFormLayout(defaultsGroup);
  defaultsForm->setSpacing(10);
  defaultsForm->setLabelAlignment(Qt::AlignRight);

  m_defaultTva = new QLineEdit(defaultsGroup);
  m_defaultTva->setPlaceholderText("20.0");
  m_defaultTva->setStyleSheet(fieldStyle);
  auto *tvaLabel = new QLabel("TVA par defaut (%)");
  tvaLabel->setStyleSheet(labelStyle);
  defaultsForm->addRow(tvaLabel, m_defaultTva);

  m_invoicePrefix = new QLineEdit(defaultsGroup);
  m_invoicePrefix->setPlaceholderText("FAC-");
  m_invoicePrefix->setStyleSheet(fieldStyle);
  auto *prefixLabel = new QLabel("Prefixe factures");
  prefixLabel->setStyleSheet(labelStyle);
  defaultsForm->addRow(prefixLabel, m_invoicePrefix);

  // Output directory with browse button
  auto *outputRow = new QWidget(defaultsGroup);
  auto *outputLayout = new QHBoxLayout(outputRow);
  outputLayout->setContentsMargins(0, 0, 0, 0);
  outputLayout->setSpacing(8);
  m_outputDir = new QLineEdit(outputRow);
  m_outputDir->setPlaceholderText("C:/Users/.../Pictures/BlackLys");
  m_outputDir->setStyleSheet(fieldStyle);
  outputLayout->addWidget(m_outputDir, 1);
  auto *browseBtn = new QPushButton("...", outputRow);
  browseBtn->setFixedSize(36, 36);
  browseBtn->setStyleSheet(
      "QPushButton { background: #27272a; color: #a1a1aa; border: none; "
      "border-radius: 6px; font-weight: 700; }"
      "QPushButton:hover { background: #3f3f46; color: white; }");
  connect(browseBtn, &QPushButton::clicked, this, [this]() {
    QString dir = QFileDialog::getExistingDirectory(this, "Dossier de sortie",
                                                    m_outputDir->text());
    if (!dir.isEmpty())
      m_outputDir->setText(dir);
  });
  outputLayout->addWidget(browseBtn);
  auto *outputLabel = new QLabel("Dossier de sortie");
  outputLabel->setStyleSheet(labelStyle);
  defaultsForm->addRow(outputLabel, outputRow);

  contentLayout->addWidget(defaultsGroup);

  // ────── Topaz AI ──────
  auto *topazGroup = new QGroupBox("Topaz AI (optionnel)", content);
  topazGroup->setStyleSheet(groupStyle);
  auto *topazForm = new QFormLayout(topazGroup);
  topazForm->setSpacing(10);
  topazForm->setLabelAlignment(Qt::AlignRight);

  auto *topazRow = new QWidget(topazGroup);
  auto *topazLayout = new QHBoxLayout(topazRow);
  topazLayout->setContentsMargins(0, 0, 0, 0);
  topazLayout->setSpacing(8);
  m_topazPath = new QLineEdit(topazRow);
  m_topazPath->setPlaceholderText(
      "C:/Program Files/Topaz Labs LLC/Topaz Photo AI/tpai.exe");
  m_topazPath->setStyleSheet(fieldStyle);
  topazLayout->addWidget(m_topazPath, 1);
  auto *topazBrowse = new QPushButton("...", topazRow);
  topazBrowse->setFixedSize(36, 36);
  topazBrowse->setStyleSheet(
      "QPushButton { background: #27272a; color: #a1a1aa; border: none; "
      "border-radius: 6px; font-weight: 700; }"
      "QPushButton:hover { background: #3f3f46; color: white; }");
  connect(topazBrowse, &QPushButton::clicked, this, [this]() {
    QString file = QFileDialog::getOpenFileName(this, "Executable Topaz AI",
                                                m_topazPath->text(),
                                                "Executables (*.exe)");
    if (!file.isEmpty())
      m_topazPath->setText(file);
  });
  topazLayout->addWidget(topazBrowse);
  auto *topazLabel = new QLabel("Chemin Topaz Photo AI");
  topazLabel->setStyleSheet(labelStyle);
  topazForm->addRow(topazLabel, topazRow);

  auto *topazInfo =
      new QLabel("Topaz Photo AI sera utilise pour le debruitage et la nettete "
                 "avancee dans le HDR Studio. Laissez vide si non installe.",
                 topazGroup);
  topazInfo->setStyleSheet(
      "color: #52525b; font-size: 11px; background: transparent;");
  topazInfo->setWordWrap(true);
  topazForm->addRow("", topazInfo);

  contentLayout->addWidget(topazGroup);

  // ────── Backup / Restore ──────
  auto *backupGroup = new QGroupBox("Sauvegarde base de donnees", content);
  backupGroup->setStyleSheet(groupStyle);
  auto *backupForm = new QHBoxLayout(backupGroup);
  backupForm->setSpacing(12);

  auto *backupInfo =
      new QLabel("Exportez ou importez une copie de la base de donnees SQLite "
                 "pour sauvegarder vos donnees.",
                 backupGroup);
  backupInfo->setStyleSheet(
      "color: #71717a; font-size: 12px; background: transparent;");
  backupInfo->setWordWrap(true);
  backupForm->addWidget(backupInfo, 1);

  auto *exportBtn = new QPushButton("Exporter", backupGroup);
  exportBtn->setFixedHeight(36);
  exportBtn->setStyleSheet(
      "QPushButton { background: rgba(16,185,129,0.1); color: #6ee7b7; "
      "border: 1px solid rgba(16,185,129,0.25); border-radius: 6px; "
      "padding: 0 20px; font-weight: 600; }"
      "QPushButton:hover { background: rgba(16,185,129,0.2); }");
  connect(exportBtn, &QPushButton::clicked, this, &SettingsPage::onBackup);
  backupForm->addWidget(exportBtn);

  auto *importBtn = new QPushButton("Importer", backupGroup);
  importBtn->setFixedHeight(36);
  importBtn->setStyleSheet(
      "QPushButton { background: rgba(239,68,68,0.1); color: #fca5a5; "
      "border: 1px solid rgba(239,68,68,0.25); border-radius: 6px; "
      "padding: 0 20px; font-weight: 600; }"
      "QPushButton:hover { background: rgba(239,68,68,0.2); }");
  connect(importBtn, &QPushButton::clicked, this, &SettingsPage::onRestore);
  backupForm->addWidget(importBtn);

  contentLayout->addWidget(backupGroup);

  contentLayout->addStretch();
  scroll->setWidget(content);
  mainLayout->addWidget(scroll, 1);

  // ── Save button ──
  auto *saveBtn = new QPushButton("Sauvegarder", this);
  saveBtn->setFixedHeight(40);
  saveBtn->setStyleSheet(
      "QPushButton { background: #f59e0b; color: white; border: none; "
      "border-radius: 8px; padding: 0 32px; font-weight: 600; "
      "font-size: 14px; }"
      "QPushButton:hover { background: #fbbf24; }");
  connect(saveBtn, &QPushButton::clicked, this, &SettingsPage::onSave);
  mainLayout->addWidget(saveBtn);
}

void SettingsPage::loadSettings() {
  m_companyName->setText(getSetting("company_name"));
  m_companyAddress->setText(getSetting("company_address"));
  m_companyPhone->setText(getSetting("company_phone"));
  m_companyEmail->setText(getSetting("company_email"));
  m_companySiret->setText(getSetting("company_siret"));
  m_defaultTva->setText(getSetting("default_tva", "20.0"));
  m_invoicePrefix->setText(getSetting("invoice_prefix", "FAC-"));
  m_outputDir->setText(getSetting("output_dir"));
  m_topazPath->setText(getSetting("topaz_path"));
}

void SettingsPage::onSave() {
  setSetting("company_name", m_companyName->text().trimmed());
  setSetting("company_address", m_companyAddress->text().trimmed());
  setSetting("company_phone", m_companyPhone->text().trimmed());
  setSetting("company_email", m_companyEmail->text().trimmed());
  setSetting("company_siret", m_companySiret->text().trimmed());
  setSetting("default_tva", m_defaultTva->text().trimmed());
  setSetting("invoice_prefix", m_invoicePrefix->text().trimmed());
  setSetting("output_dir", m_outputDir->text().trimmed());
  setSetting("topaz_path", m_topazPath->text().trimmed());

  ToastWidget::show(window(), "Parametres sauvegardes", ToastWidget::Success);
}

void SettingsPage::refresh() { loadSettings(); }

void SettingsPage::onBackup() {
  QString dbPath = Database::instance().db().databaseName();
  QString dest = QFileDialog::getSaveFileName(
      this, "Exporter base de donnees", "blacklys_backup.db", "SQLite (*.db)");
  if (dest.isEmpty())
    return;

  if (QFile::exists(dest))
    QFile::remove(dest);

  if (QFile::copy(dbPath, dest)) {
    ToastWidget::show(window(), "Sauvegarde exportee avec succes",
                      ToastWidget::Success);
  } else {
    ToastWidget::show(window(), "Erreur lors de l'export", ToastWidget::Error);
  }
}

void SettingsPage::onRestore() {
  auto reply = QMessageBox::warning(
      this, "Importer",
      "Attention : cette operation va remplacer toutes les donnees "
      "actuelles. Continuer ?",
      QMessageBox::Yes | QMessageBox::No);
  if (reply != QMessageBox::Yes)
    return;

  QString src = QFileDialog::getOpenFileName(this, "Importer base de donnees",
                                             "", "SQLite (*.db)");
  if (src.isEmpty())
    return;

  QString dbPath = Database::instance().db().databaseName();

  // Close the database connection before touching the file
  {
    QString connName = Database::instance().db().connectionName();
    Database::instance().db().close();
    QSqlDatabase::removeDatabase(connName);
  }

  bool ok = false;
  if (QFile::remove(dbPath) && QFile::copy(src, dbPath)) {
    ok = true;
  }

  // Re-initialize the database (reopens connection)
  Database::instance().initialize(dbPath);

  if (ok) {
    loadSettings();
    ToastWidget::show(window(), "Base de donnees restauree",
                      ToastWidget::Success);
  } else {
    ToastWidget::show(window(), "Erreur lors de l'import", ToastWidget::Error);
  }
}
