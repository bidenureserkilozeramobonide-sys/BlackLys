#include "ClientsPage.h"
#include "../database/ActivityLog.h"
#include "../database/ClientModel.h"
#include "../database/InvoiceModel.h"
#include "../database/MissionModel.h"
#include "../database/QuoteModel.h"
#include "../widgets/PageHeader.h"

#include <QCheckBox>
#include <QDate>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QScrollArea>
#include <QTabWidget>
#include <QTextEdit>
#include <QTextStream>
#include <QVBoxLayout>

// ── Available tags ──
static const QStringList TAG_OPTIONS = {"VIP", "Prospect", "Fidele", "Pro",
                                        "Particulier"};

static QColor tagColor(const QString &tag) {
  if (tag == "VIP")
    return QColor("#f59e0b");
  if (tag == "Prospect")
    return QColor("#3b82f6");
  if (tag == "Fidele")
    return QColor("#10b981");
  if (tag == "Pro")
    return QColor("#8b5cf6");
  if (tag == "Particulier")
    return QColor("#6b7280");
  return QColor("#52525b");
}

ClientsPage::ClientsPage(QWidget *parent) : QWidget(parent) {
  setObjectName("page");
  setupUi();
  refresh();
}

QColor ClientsPage::avatarColor(const QString &name) const {
  // Deterministic color from name hash
  static const QColor palette[] = {
      QColor("#f59e0b"), QColor("#10b981"), QColor("#3b82f6"),
      QColor("#8b5cf6"), QColor("#ec4899"), QColor("#ef4444"),
      QColor("#06b6d4"), QColor("#84cc16"),
  };
  uint hash = 0;
  for (const auto &ch : name)
    hash = hash * 31 + ch.unicode();
  return palette[hash % 8];
}

QString ClientsPage::initials(const QString &name) const {
  QStringList parts = name.trimmed().split(' ', Qt::SkipEmptyParts);
  if (parts.isEmpty())
    return "?";
  if (parts.size() == 1)
    return parts[0].left(1).toUpper();
  return (parts[0].left(1) + parts.last().left(1)).toUpper();
}

void ClientsPage::setupUi() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  auto *header = new PageHeader("Clients", "Gestion de votre clientele", this);
  layout->addWidget(header);

  // ── Scrollable content ──
  auto *scrollArea = new QScrollArea(this);
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setStyleSheet(
      "QScrollArea { background: transparent; border: none; }");

  auto *content = new QWidget(scrollArea);
  content->setObjectName("pageContent");
  auto *contentLayout = new QVBoxLayout(content);
  contentLayout->setContentsMargins(32, 24, 32, 24);
  contentLayout->setSpacing(16);

  // ── Stat Badges Row ──
  auto *statsRow = new QWidget(content);
  auto *statsLayout = new QHBoxLayout(statsRow);
  statsLayout->setContentsMargins(0, 0, 0, 0);
  statsLayout->setSpacing(10);

  auto makeStatBadge = [&](const QString &label,
                           const QString &color) -> QLabel * {
    auto *badge = new QLabel(label, statsRow);
    QColor c(color);
    badge->setStyleSheet(QString("background: rgba(%1,%2,%3,30); color: %4; "
                                 "border-radius: 4px; padding: 4px 12px; "
                                 "font-size: 12px; font-weight: 600;")
                             .arg(c.red())
                             .arg(c.green())
                             .arg(c.blue())
                             .arg(color));
    return badge;
  };

  m_totalBadge = makeStatBadge("Total: 0", "#f59e0b");
  statsLayout->addWidget(m_totalBadge);

  m_monthBadge = makeStatBadge("Ce mois: 0", "#10b981");
  statsLayout->addWidget(m_monthBadge);

  m_companyBadge = makeStatBadge("Avec societe: 0", "#8b5cf6");
  statsLayout->addWidget(m_companyBadge);

  statsLayout->addStretch();

  // Result counter
  m_resultCount = new QLabel("", statsRow);
  m_resultCount->setStyleSheet(
      "color: #52525b; font-size: 12px; background: transparent;");
  statsLayout->addWidget(m_resultCount);

  contentLayout->addWidget(statsRow);

  // ── Toolbar ──
  auto *toolbar = new QWidget(content);
  auto *toolbarLayout = new QHBoxLayout(toolbar);
  toolbarLayout->setContentsMargins(0, 0, 0, 0);
  toolbarLayout->setSpacing(12);

  m_searchInput = new QLineEdit(toolbar);
  m_searchInput->setPlaceholderText(
      QString("  %1  Rechercher un client...").arg(QChar(0xE721)));
  m_searchInput->setFixedHeight(40);
  m_searchInput->setMinimumWidth(260);
  connect(m_searchInput, &QLineEdit::textChanged, this, &ClientsPage::onSearch);
  toolbarLayout->addWidget(m_searchInput);

  // ── Company filter ──
  m_companyFilter = new QComboBox(toolbar);
  m_companyFilter->addItem("Toutes societes");
  m_companyFilter->setFixedHeight(40);
  m_companyFilter->setFixedWidth(180);
  connect(m_companyFilter, &QComboBox::currentIndexChanged, this,
          &ClientsPage::onFilterChanged);
  toolbarLayout->addWidget(m_companyFilter);

  toolbarLayout->addStretch();

  auto *addBtn = new QPushButton(
      QString("  %1  Nouveau client").arg(QChar(0xE710)), toolbar);
  addBtn->setObjectName("primaryButton");
  addBtn->setFixedHeight(40);
  addBtn->setCursor(Qt::PointingHandCursor);
  connect(addBtn, &QPushButton::clicked, this, &ClientsPage::onAddClient);
  toolbarLayout->addWidget(addBtn);

  auto *viewBtn = new QPushButton(
      QString("  %1  Fiche client").arg(QChar(0xE716)), toolbar);
  viewBtn->setFixedHeight(40);
  viewBtn->setCursor(Qt::PointingHandCursor);
  connect(viewBtn, &QPushButton::clicked, this, [this]() {
    int row = m_table->currentRow();
    if (row >= 0)
      onViewClient(row);
  });
  toolbarLayout->addWidget(viewBtn);

  auto *importBtn = new QPushButton(
      QString("  %1  Importer CSV").arg(QChar(0xE8B5)), toolbar);
  importBtn->setFixedHeight(40);
  importBtn->setCursor(Qt::PointingHandCursor);
  connect(importBtn, &QPushButton::clicked, this, &ClientsPage::onImportCsv);
  toolbarLayout->addWidget(importBtn);

  auto *csvBtn = new QPushButton(
      QString("  %1  Exporter CSV").arg(QChar(0xE896)), toolbar);
  csvBtn->setFixedHeight(40);
  csvBtn->setCursor(Qt::PointingHandCursor);
  connect(csvBtn, &QPushButton::clicked, this, &ClientsPage::onExportCsv);
  toolbarLayout->addWidget(csvBtn);

  auto *batchDeleteBtn = new QPushButton(
      QString("  %1  Supprimer selection").arg(QChar(0xE74D)), toolbar);
  batchDeleteBtn->setFixedHeight(40);
  batchDeleteBtn->setCursor(Qt::PointingHandCursor);
  batchDeleteBtn->setStyleSheet(
      "QPushButton { color: #ef4444; background: rgba(239,68,68,0.1); "
      "border: 1px solid rgba(239,68,68,0.2); border-radius: 8px; "
      "padding: 0 16px; font-weight: 500; }"
      "QPushButton:hover { background: rgba(239,68,68,0.2); }");
  connect(batchDeleteBtn, &QPushButton::clicked, this,
          &ClientsPage::onBatchDelete);
  toolbarLayout->addWidget(batchDeleteBtn);

  contentLayout->addWidget(toolbar);

  // ── Table ──
  m_table = new QTableWidget(content);
  m_table->setColumnCount(
      8); // Avatar, Name, Email, Phone, Company, Tags, Date, Actions
  m_table->setHorizontalHeaderLabels(
      {"", "Nom", "Email", "Telephone", "Societe", "Tags", "Cree le", ""});
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setSelectionMode(QAbstractItemView::MultiSelection);
  m_table->setAlternatingRowColors(true);
  m_table->verticalHeader()->hide();
  m_table->setShowGrid(false);
  m_table->setSortingEnabled(true);
  m_table->horizontalHeader()->setStretchLastSection(false);
  m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
  m_table->setColumnWidth(0, 48);
  m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  m_table->horizontalHeader()->setSectionResizeMode(
      2, QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(
      3, QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(
      4, QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(
      5, QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(
      6, QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(
      7, QHeaderView::ResizeToContents);
  m_table->setMinimumHeight(300);
  m_table->setStyleSheet(
      "QTableWidget { background: #0f0f11; border: 1px solid #18181b; "
      "border-radius: 8px; color: #e4e4e7; }"
      "QTableWidget::item { padding: 8px; }"
      "QTableWidget::item:selected { background: rgba(245,158,11,0.12); }"
      "QHeaderView::section { background: #0f0f11; color: #71717a; "
      "border: none; border-bottom: 1px solid #18181b; padding: 8px 12px; "
      "font-weight: 600; font-size: 11px; }");

  connect(m_table, &QTableWidget::cellDoubleClicked, this,
          [this](int row, int /*col*/) { onEditClient(row); });

  contentLayout->addWidget(m_table, 1);

  scrollArea->setWidget(content);
  layout->addWidget(scrollArea, 1);
}

void ClientsPage::populateTable(const QString &search) {
  auto clients =
      search.isEmpty() ? ClientModel::all() : ClientModel::search(search);

  // Filter by company if selected
  QString selectedCompany;
  if (m_companyFilter && m_companyFilter->currentIndex() > 0) {
    selectedCompany = m_companyFilter->currentText();
  }
  if (!selectedCompany.isEmpty()) {
    QList<Client> filtered;
    for (const auto &c : clients) {
      if (c.company == selectedCompany)
        filtered.append(c);
    }
    clients = filtered;
  }

  // Disable sorting during population to avoid issues
  m_table->setSortingEnabled(false);
  m_table->setRowCount(clients.size());

  if (clients.isEmpty()) {
    m_table->setRowCount(0);
    m_resultCount->setText("Aucun client");
    m_table->setSortingEnabled(true);
    return;
  }

  m_resultCount->setText(QString("%1 client(s)").arg(clients.size()));

  for (int i = 0; i < clients.size(); ++i) {
    const auto &c = clients[i];

    // ── Avatar column ──
    auto *avatarWidget = new QWidget();
    auto *avatarLayout = new QHBoxLayout(avatarWidget);
    avatarLayout->setContentsMargins(4, 4, 4, 4);
    avatarLayout->setAlignment(Qt::AlignCenter);

    auto *avatarLabel = new QLabel(initials(c.name), avatarWidget);
    QColor bg = avatarColor(c.name);
    avatarLabel->setFixedSize(32, 32);
    avatarLabel->setAlignment(Qt::AlignCenter);
    avatarLabel->setStyleSheet(
        QString("background: rgba(%1,%2,%3,40); color: %4; "
                "border-radius: 16px; font-size: 12px; font-weight: 700;")
            .arg(bg.red())
            .arg(bg.green())
            .arg(bg.blue())
            .arg(bg.name()));
    avatarLayout->addWidget(avatarLabel);
    m_table->setCellWidget(i, 0, avatarWidget);

    // Need a sortable item behind the widget
    auto *avatarItem = new QTableWidgetItem(c.name);
    avatarItem->setData(Qt::UserRole, c.id);
    m_table->setItem(i, 0, avatarItem);

    // Name — bold amber
    auto *nameItem = new QTableWidgetItem(c.name);
    nameItem->setData(Qt::UserRole, c.id);
    nameItem->setForeground(QColor("#fbbf24"));
    QFont nameFont;
    nameFont.setBold(true);
    nameItem->setFont(nameFont);
    m_table->setItem(i, 1, nameItem);

    m_table->setItem(i, 2, new QTableWidgetItem(c.email));
    m_table->setItem(i, 3, new QTableWidgetItem(c.phone));

    // Company — subtle purple
    auto *companyItem = new QTableWidgetItem(c.company);
    if (!c.company.isEmpty())
      companyItem->setForeground(QColor("#a78bfa"));
    m_table->setItem(i, 4, companyItem);

    // ── Tags column ──
    if (!c.tags.trimmed().isEmpty()) {
      auto *tagsWidget = new QWidget();
      auto *tagsLayout = new QHBoxLayout(tagsWidget);
      tagsLayout->setContentsMargins(4, 4, 4, 4);
      tagsLayout->setSpacing(4);

      QStringList tagList = c.tags.split(',', Qt::SkipEmptyParts);
      for (const auto &tag : tagList) {
        QString t = tag.trimmed();
        QColor tc = tagColor(t);
        auto *tagLabel = new QLabel(t, tagsWidget);
        tagLabel->setStyleSheet(
            QString("background: rgba(%1,%2,%3,25); color: %4; "
                    "border-radius: 3px; padding: 1px 6px; "
                    "font-size: 10px; font-weight: 600;")
                .arg(tc.red())
                .arg(tc.green())
                .arg(tc.blue())
                .arg(tc.name()));
        tagsLayout->addWidget(tagLabel);
      }
      tagsLayout->addStretch();
      m_table->setCellWidget(i, 5, tagsWidget);
    }
    // Sortable item for tags column
    m_table->setItem(i, 5, new QTableWidgetItem(c.tags));

    m_table->setItem(i, 6,
                     new QTableWidgetItem(c.createdAt.toString("dd/MM/yyyy")));

    // ── Inline action buttons ──
    auto *actionsWidget = new QWidget();
    auto *actionsLayout = new QHBoxLayout(actionsWidget);
    actionsLayout->setContentsMargins(4, 0, 4, 0);
    actionsLayout->setSpacing(4);

    auto *editBtn = new QPushButton("Modifier");
    editBtn->setFixedSize(80, 28);
    editBtn->setCursor(Qt::PointingHandCursor);
    editBtn->setStyleSheet(
        "QPushButton { background: #27272a; color: #a1a1aa; border: none; "
        "border-radius: 6px; font-size: 11px; }"
        "QPushButton:hover { background: #3f3f46; color: white; }");
    connect(editBtn, &QPushButton::clicked, this,
            [this, i]() { onEditClient(i); });
    actionsLayout->addWidget(editBtn);

    auto *viewActionBtn = new QPushButton(QString(QChar(0xE716)));
    viewActionBtn->setFixedSize(28, 28);
    viewActionBtn->setCursor(Qt::PointingHandCursor);
    viewActionBtn->setToolTip("Fiche client");
    viewActionBtn->setStyleSheet(
        "QPushButton { background: #27272a; color: #fbbf24; border: none; "
        "border-radius: 6px; font-size: 12px; "
        "font-family: 'Segoe MDL2 Assets'; }"
        "QPushButton:hover { background: #3f3f46; }");
    connect(viewActionBtn, &QPushButton::clicked, this,
            [this, i]() { onViewClient(i); });
    actionsLayout->addWidget(viewActionBtn);

    auto *delBtn = new QPushButton("\xE2\x9C\x95"); // ✕
    delBtn->setFixedSize(28, 28);
    delBtn->setCursor(Qt::PointingHandCursor);
    delBtn->setToolTip("Supprimer");
    delBtn->setStyleSheet(
        "QPushButton { background: #27272a; color: #ef4444; border: none; "
        "border-radius: 6px; font-size: 12px; }"
        "QPushButton:hover { background: #451a1a; }");
    connect(delBtn, &QPushButton::clicked, this, [this, i]() {
      if (!m_table->item(i, 1))
        return;
      int id = m_table->item(i, 1)->data(Qt::UserRole).toInt();
      QString name = m_table->item(i, 1)->text();
      auto reply = QMessageBox::warning(
          this, "Supprimer", QString("Supprimer le client \"%1\" ?").arg(name),
          QMessageBox::Yes | QMessageBox::No);
      if (reply == QMessageBox::Yes) {
        ClientModel::remove(id);
        ActivityLog::log("delete", "client", "Client: " + name);
        refresh();
      }
    });
    actionsLayout->addWidget(delBtn);

    m_table->setCellWidget(i, 7, actionsWidget);

    m_table->setRowHeight(i, 48);
  }

  m_table->setSortingEnabled(true);
}

void ClientsPage::updateStats() {
  auto allClients = ClientModel::all();
  int total = allClients.size();

  QDate firstOfMonth(QDate::currentDate().year(), QDate::currentDate().month(),
                     1);
  int thisMonth = 0;
  int withCompany = 0;
  for (const auto &c : allClients) {
    if (c.createdAt.date() >= firstOfMonth)
      ++thisMonth;
    if (!c.company.trimmed().isEmpty())
      ++withCompany;
  }

  if (m_totalBadge)
    m_totalBadge->setText(QString("Total: %1").arg(total));
  if (m_monthBadge)
    m_monthBadge->setText(QString("Ce mois: %1").arg(thisMonth));
  if (m_companyBadge)
    m_companyBadge->setText(QString("Avec societe: %1").arg(withCompany));
}

void ClientsPage::refreshCompanyFilter() {
  if (!m_companyFilter)
    return;
  QString current = m_companyFilter->currentText();
  m_companyFilter->blockSignals(true);
  m_companyFilter->clear();
  m_companyFilter->addItem("Toutes societes");
  auto companies = ClientModel::distinctCompanies();
  for (const auto &c : companies) {
    m_companyFilter->addItem(c);
  }
  // Restore selection
  int idx = m_companyFilter->findText(current);
  m_companyFilter->setCurrentIndex(idx > 0 ? idx : 0);
  m_companyFilter->blockSignals(false);
}

void ClientsPage::refresh() {
  updateStats();
  refreshCompanyFilter();
  populateTable(m_searchInput ? m_searchInput->text() : "");
}

void ClientsPage::onSearch(const QString &text) { populateTable(text); }

void ClientsPage::onFilterChanged() {
  populateTable(m_searchInput ? m_searchInput->text() : "");
}

// ══════════════════════════════════════════════
//  Tag selector helper for dialogs
// ══════════════════════════════════════════════
static QWidget *createTagSelector(QWidget *parent, const QString &currentTags) {
  auto *container = new QWidget(parent);
  auto *layout = new QHBoxLayout(container);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  QStringList activeTags = currentTags.split(',', Qt::SkipEmptyParts);
  for (const auto &tag : TAG_OPTIONS) {
    auto *cb = new QCheckBox(tag, container);
    cb->setObjectName("tagCheck_" + tag);
    cb->setChecked(activeTags.contains(tag, Qt::CaseInsensitive));
    QColor tc = tagColor(tag);
    cb->setStyleSheet(
        QString("QCheckBox { color: %1; font-size: 12px; font-weight: 600; "
                "background: transparent; }"
                "QCheckBox::indicator { width: 14px; height: 14px; "
                "border-radius: 3px; border: 1px solid %1; }"
                "QCheckBox::indicator:checked { background: %1; }")
            .arg(tc.name()));
    layout->addWidget(cb);
  }
  layout->addStretch();
  return container;
}

static QString getTagsFromSelector(QWidget *selector) {
  QStringList tags;
  for (const auto &tag : TAG_OPTIONS) {
    auto *cb = selector->findChild<QCheckBox *>("tagCheck_" + tag);
    if (cb && cb->isChecked())
      tags.append(tag);
  }
  return tags.join(",");
}

void ClientsPage::onAddClient() {
  QDialog dialog(this);
  dialog.setWindowTitle("Nouveau client");
  dialog.setMinimumWidth(500);
  dialog.setStyleSheet(
      "QDialog { background-color: #09090b; }"
      "QLabel { color: #a1a1aa; background: transparent; }"
      "QLineEdit, QTextEdit { background: #18181b; color: #e4e4e7; "
      "  border: 1px solid #27272a; border-radius: 6px; padding: 6px; }"
      "QLineEdit:focus, QTextEdit:focus { border-color: #f59e0b; }");

  auto *form = new QFormLayout(&dialog);
  form->setContentsMargins(24, 24, 24, 24);
  form->setSpacing(12);

  auto *nameEdit = new QLineEdit(&dialog);
  nameEdit->setPlaceholderText("Nom complet");
  form->addRow("Nom *", nameEdit);

  auto *emailEdit = new QLineEdit(&dialog);
  emailEdit->setPlaceholderText("email@example.com");
  form->addRow("Email", emailEdit);

  auto *phoneEdit = new QLineEdit(&dialog);
  phoneEdit->setPlaceholderText("+33 6 00 00 00 00");
  form->addRow("Telephone", phoneEdit);

  auto *companyEdit = new QLineEdit(&dialog);
  companyEdit->setPlaceholderText("Nom de la societe");
  form->addRow("Societe", companyEdit);

  auto *addressEdit = new QLineEdit(&dialog);
  addressEdit->setPlaceholderText("Adresse complete");
  form->addRow("Adresse", addressEdit);

  auto *tagSelector = createTagSelector(&dialog, "");
  form->addRow("Tags", tagSelector);

  auto *notesEdit = new QTextEdit(&dialog);
  notesEdit->setPlaceholderText("Notes, commentaires...");
  notesEdit->setFixedHeight(80);
  form->addRow("Notes", notesEdit);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  buttons->button(QDialogButtonBox::Ok)->setText("Creer");
  buttons->button(QDialogButtonBox::Ok)
      ->setStyleSheet(
          "QPushButton { background: #f59e0b; color: white; border: none; "
          "border-radius: 6px; padding: 8px 20px; font-weight: 600; }"
          "QPushButton:hover { background: #fbbf24; }");
  buttons->button(QDialogButtonBox::Cancel)->setText("Annuler");
  form->addRow(buttons);

  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  if (dialog.exec() == QDialog::Accepted &&
      !nameEdit->text().trimmed().isEmpty()) {
    Client c;
    c.name = nameEdit->text().trimmed();
    c.email = emailEdit->text().trimmed();
    c.phone = phoneEdit->text().trimmed();
    c.company = companyEdit->text().trimmed();
    c.address = addressEdit->text().trimmed();
    c.notes = notesEdit->toPlainText().trimmed();
    c.tags = getTagsFromSelector(tagSelector);

    ClientModel::create(c);
    ActivityLog::log("create", "client", "Client: " + c.name);
    refresh();
  }
}

void ClientsPage::onEditClient(int row) {
  if (!m_table->item(row, 1))
    return;
  int clientId = m_table->item(row, 1)->data(Qt::UserRole).toInt();
  Client c = ClientModel::getById(clientId);
  if (c.id == 0)
    return;

  QDialog dialog(this);
  dialog.setWindowTitle("Modifier client");
  dialog.setMinimumWidth(500);
  dialog.setStyleSheet(
      "QDialog { background-color: #09090b; }"
      "QLabel { color: #a1a1aa; background: transparent; }"
      "QLineEdit, QTextEdit { background: #18181b; color: #e4e4e7; "
      "  border: 1px solid #27272a; border-radius: 6px; padding: 6px; }"
      "QLineEdit:focus, QTextEdit:focus { border-color: #f59e0b; }");

  auto *form = new QFormLayout(&dialog);
  form->setContentsMargins(24, 24, 24, 24);
  form->setSpacing(12);

  auto *nameEdit = new QLineEdit(c.name, &dialog);
  form->addRow("Nom *", nameEdit);

  auto *emailEdit = new QLineEdit(c.email, &dialog);
  form->addRow("Email", emailEdit);

  auto *phoneEdit = new QLineEdit(c.phone, &dialog);
  form->addRow("Telephone", phoneEdit);

  auto *companyEdit = new QLineEdit(c.company, &dialog);
  form->addRow("Societe", companyEdit);

  auto *addressEdit = new QLineEdit(c.address, &dialog);
  form->addRow("Adresse", addressEdit);

  auto *tagSelector = createTagSelector(&dialog, c.tags);
  form->addRow("Tags", tagSelector);

  auto *notesEdit = new QTextEdit(&dialog);
  notesEdit->setPlainText(c.notes);
  notesEdit->setPlaceholderText("Notes, commentaires...");
  notesEdit->setFixedHeight(80);
  form->addRow("Notes", notesEdit);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  buttons->button(QDialogButtonBox::Ok)->setText("Enregistrer");
  buttons->button(QDialogButtonBox::Ok)
      ->setStyleSheet(
          "QPushButton { background: #f59e0b; color: white; border: none; "
          "border-radius: 6px; padding: 8px 20px; font-weight: 600; }"
          "QPushButton:hover { background: #fbbf24; }");
  buttons->button(QDialogButtonBox::Cancel)->setText("Annuler");

  // Delete button
  auto *deleteBtn = new QPushButton("Supprimer", &dialog);
  deleteBtn->setStyleSheet(
      "QPushButton { color: #ef4444; background: rgba(239,68,68,0.1); "
      "border: 1px solid rgba(239,68,68,0.2); border-radius: 6px; "
      "padding: 8px 16px; }"
      "QPushButton:hover { background: rgba(239,68,68,0.2); }");
  buttons->addButton(deleteBtn, QDialogButtonBox::DestructiveRole);
  connect(deleteBtn, &QPushButton::clicked, [&]() { dialog.done(2); });

  form->addRow(buttons);

  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  int result = dialog.exec();
  if (result == QDialog::Accepted) {
    c.name = nameEdit->text().trimmed();
    c.email = emailEdit->text().trimmed();
    c.phone = phoneEdit->text().trimmed();
    c.company = companyEdit->text().trimmed();
    c.address = addressEdit->text().trimmed();
    c.notes = notesEdit->toPlainText().trimmed();
    c.tags = getTagsFromSelector(tagSelector);
    ClientModel::update(c);
    ActivityLog::log("edit", "client", "Client: " + c.name);
    refresh();
  } else if (result == 2) {
    auto reply = QMessageBox::warning(
        this, "Supprimer", QString("Supprimer le client \"%1\" ?").arg(c.name),
        QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
      ClientModel::remove(c.id);
      ActivityLog::log("delete", "client", "Client: " + c.name);
      refresh();
    }
  }
}

void ClientsPage::onDeleteClient() {
  int row = m_table->currentRow();
  if (row < 0)
    return;
  onEditClient(row);
}

void ClientsPage::onViewClient(int row) {
  if (!m_table->item(row, 1))
    return;
  int clientId = m_table->item(row, 1)->data(Qt::UserRole).toInt();
  Client client = ClientModel::getById(clientId);
  if (client.id == 0)
    return;

  QDialog dlg(this);
  dlg.setWindowTitle(QString("Fiche client : %1").arg(client.name));
  dlg.setMinimumSize(750, 550);
  dlg.setStyleSheet("QDialog { background-color: #09090b; }"
                    "QLabel { color: #a1a1aa; background: transparent; }");

  auto *mainLayout = new QVBoxLayout(&dlg);
  mainLayout->setContentsMargins(24, 24, 24, 24);
  mainLayout->setSpacing(16);

  // ── Client Title + Avatar ──
  auto *titleRow = new QWidget(&dlg);
  auto *titleLayout = new QHBoxLayout(titleRow);
  titleLayout->setContentsMargins(0, 0, 0, 0);
  titleLayout->setSpacing(16);

  // Large avatar
  auto *avatarLabel = new QLabel(initials(client.name), titleRow);
  QColor bg = avatarColor(client.name);
  avatarLabel->setFixedSize(48, 48);
  avatarLabel->setAlignment(Qt::AlignCenter);
  avatarLabel->setStyleSheet(
      QString("background: rgba(%1,%2,%3,40); color: %4; "
              "border-radius: 24px; font-size: 18px; font-weight: 700;")
          .arg(bg.red())
          .arg(bg.green())
          .arg(bg.blue())
          .arg(bg.name()));
  titleLayout->addWidget(avatarLabel);

  auto *titleCol = new QWidget(titleRow);
  auto *titleColLayout = new QVBoxLayout(titleCol);
  titleColLayout->setContentsMargins(0, 0, 0, 0);
  titleColLayout->setSpacing(4);

  auto *titleLabel = new QLabel(client.name, titleCol);
  titleLabel->setStyleSheet(
      "color: #fafafa; font-size: 20px; font-weight: 700;");
  titleColLayout->addWidget(titleLabel);

  // Tags in title area
  if (!client.tags.trimmed().isEmpty()) {
    auto *tagsRow = new QWidget(titleCol);
    auto *tagsLay = new QHBoxLayout(tagsRow);
    tagsLay->setContentsMargins(0, 0, 0, 0);
    tagsLay->setSpacing(6);
    QStringList tagList = client.tags.split(',', Qt::SkipEmptyParts);
    for (const auto &tag : tagList) {
      QString t = tag.trimmed();
      QColor tc = tagColor(t);
      auto *badge = new QLabel(t, tagsRow);
      badge->setStyleSheet(QString("background: rgba(%1,%2,%3,25); color: %4; "
                                   "border-radius: 3px; padding: 2px 8px; "
                                   "font-size: 11px; font-weight: 600;")
                               .arg(tc.red())
                               .arg(tc.green())
                               .arg(tc.blue())
                               .arg(tc.name()));
      tagsLay->addWidget(badge);
    }
    tagsLay->addStretch();
    titleColLayout->addWidget(tagsRow);
  }

  titleLayout->addWidget(titleCol, 1);
  mainLayout->addWidget(titleRow);

  // ── Client info card ──
  auto *infoCard = new QWidget(&dlg);
  infoCard->setStyleSheet(
      "background: rgba(255,255,255,0.02); border-radius: 12px;");
  auto *infoLayout = new QHBoxLayout(infoCard);
  infoLayout->setContentsMargins(20, 16, 20, 16);
  infoLayout->setSpacing(24);

  auto addInfoCol = [&](const QString &label, const QString &value) {
    auto *col = new QWidget(infoCard);
    auto *colLay = new QVBoxLayout(col);
    colLay->setContentsMargins(0, 0, 0, 0);
    colLay->setSpacing(2);
    auto *lblW = new QLabel(label, col);
    lblW->setStyleSheet("color: #52525b; font-size: 11px;");
    auto *valW = new QLabel(
        value.isEmpty() ? QString::fromUtf8("\xe2\x80\x94") : value, col);
    valW->setStyleSheet("color: #e4e4e7; font-size: 13px; font-weight: 500;");
    colLay->addWidget(lblW);
    colLay->addWidget(valW);
    infoLayout->addWidget(col);
  };

  addInfoCol("Email", client.email);
  addInfoCol("Telephone", client.phone);
  addInfoCol("Societe", client.company);
  addInfoCol("Adresse", client.address);
  infoLayout->addStretch();
  mainLayout->addWidget(infoCard);

  // Tabs: Missions | Factures | Devis | Notes
  auto *tabs = new QTabWidget(&dlg);

  // ── Missions tab
  auto *missionsTab = new QTableWidget();
  missionsTab->setColumnCount(5);
  missionsTab->setHorizontalHeaderLabels(
      {"Titre", "Date", "Type", "Statut", "Photos"});
  missionsTab->setEditTriggers(QAbstractItemView::NoEditTriggers);
  missionsTab->setSelectionBehavior(QAbstractItemView::SelectRows);
  missionsTab->verticalHeader()->hide();
  missionsTab->setShowGrid(false);
  missionsTab->horizontalHeader()->setStretchLastSection(true);
  missionsTab->horizontalHeader()->setSectionResizeMode(0,
                                                        QHeaderView::Stretch);

  auto missions = MissionModel::byClientId(clientId);
  missionsTab->setRowCount(missions.size());
  for (int i = 0; i < missions.size(); ++i) {
    const auto &m = missions[i];
    missionsTab->setItem(i, 0, new QTableWidgetItem(m.title));
    missionsTab->setItem(i, 1, new QTableWidgetItem(m.date));
    missionsTab->setItem(i, 2, new QTableWidgetItem(m.type));
    auto *si = new QTableWidgetItem(m.status);
    QString statusColor = m.status == "terminee"   ? "#10b981"
                          : m.status == "en_cours" ? "#3b82f6"
                                                   : "#f59e0b";
    si->setForeground(QColor(statusColor));
    missionsTab->setItem(i, 3, si);
    missionsTab->setItem(i, 4,
                         new QTableWidgetItem(QString::number(m.photoCount)));
    missionsTab->setRowHeight(i, 36);
  }
  tabs->addTab(missionsTab, QString("Missions (%1)").arg(missions.size()));

  // ── Factures tab
  auto *invoicesTab = new QTableWidget();
  invoicesTab->setColumnCount(4);
  invoicesTab->setHorizontalHeaderLabels({"Numero", "Date", "Total", "Statut"});
  invoicesTab->setEditTriggers(QAbstractItemView::NoEditTriggers);
  invoicesTab->setSelectionBehavior(QAbstractItemView::SelectRows);
  invoicesTab->verticalHeader()->hide();
  invoicesTab->setShowGrid(false);
  invoicesTab->horizontalHeader()->setStretchLastSection(true);
  invoicesTab->horizontalHeader()->setSectionResizeMode(0,
                                                        QHeaderView::Stretch);

  auto invoices = InvoiceModel::byClientId(clientId);
  invoicesTab->setRowCount(invoices.size());
  for (int i = 0; i < invoices.size(); ++i) {
    const auto &inv = invoices[i];
    auto *numItem = new QTableWidgetItem(inv.number);
    numItem->setForeground(QColor("#fbbf24"));
    QFont numFont;
    numFont.setBold(true);
    numItem->setFont(numFont);
    invoicesTab->setItem(i, 0, numItem);
    invoicesTab->setItem(i, 1, new QTableWidgetItem(inv.date));

    auto *amountItem =
        new QTableWidgetItem(QString("%1 EUR").arg(inv.total, 0, 'f', 2));
    amountItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    if (inv.total > 0)
      amountItem->setForeground(QColor("#10b981"));
    QFont amtFont;
    amtFont.setBold(true);
    amountItem->setFont(amtFont);
    invoicesTab->setItem(i, 2, amountItem);

    auto *si = new QTableWidgetItem(inv.status);
    si->setForeground(inv.status == "payee"     ? QColor("#10b981")
                      : inv.status == "envoyee" ? QColor("#f59e0b")
                                                : QColor("#6b7280"));
    invoicesTab->setItem(i, 3, si);
    invoicesTab->setRowHeight(i, 36);
  }
  tabs->addTab(invoicesTab, QString("Factures (%1)").arg(invoices.size()));

  // ── Devis tab
  auto *devisTab = new QTableWidget();
  devisTab->setColumnCount(4);
  devisTab->setHorizontalHeaderLabels({"Numero", "Date", "Total", "Statut"});
  devisTab->setEditTriggers(QAbstractItemView::NoEditTriggers);
  devisTab->setSelectionBehavior(QAbstractItemView::SelectRows);
  devisTab->verticalHeader()->hide();
  devisTab->setShowGrid(false);
  devisTab->horizontalHeader()->setStretchLastSection(true);
  devisTab->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

  auto quotes = QuoteModel::byClientId(clientId);
  devisTab->setRowCount(quotes.size());
  for (int i = 0; i < quotes.size(); ++i) {
    const auto &q = quotes[i];
    auto *numItem = new QTableWidgetItem(q.number);
    numItem->setForeground(QColor("#fbbf24"));
    QFont numFont;
    numFont.setBold(true);
    numItem->setFont(numFont);
    devisTab->setItem(i, 0, numItem);
    devisTab->setItem(i, 1, new QTableWidgetItem(q.date));
    devisTab->setItem(
        i, 2, new QTableWidgetItem(QString("%1 EUR").arg(q.total, 0, 'f', 2)));
    auto *si = new QTableWidgetItem(q.status);
    si->setForeground(q.status == "acceptee"  ? QColor("#10b981")
                      : q.status == "envoyee" ? QColor("#f59e0b")
                                              : QColor("#6b7280"));
    devisTab->setItem(i, 3, si);
    devisTab->setRowHeight(i, 36);
  }
  tabs->addTab(devisTab, QString("Devis (%1)").arg(quotes.size()));

  // ── Notes tab
  if (!client.notes.trimmed().isEmpty()) {
    auto *notesWidget = new QWidget();
    auto *notesLayout = new QVBoxLayout(notesWidget);
    notesLayout->setContentsMargins(16, 16, 16, 16);
    auto *notesLabel = new QLabel(client.notes, notesWidget);
    notesLabel->setStyleSheet(
        "color: #d4d4d8; font-size: 13px; line-height: 1.6;");
    notesLabel->setWordWrap(true);
    notesLayout->addWidget(notesLabel);
    notesLayout->addStretch();
    tabs->addTab(notesWidget, "Notes");
  }

  mainLayout->addWidget(tabs, 1);

  // ── Revenue summary ──
  double totalRevenue = 0;
  for (const auto &inv : invoices) {
    if (inv.status == "payee")
      totalRevenue += inv.total;
  }
  if (totalRevenue > 0 || !invoices.isEmpty()) {
    auto *revCard = new QWidget(&dlg);
    revCard->setStyleSheet(
        "background: rgba(16,185,129,0.06); border: 1px solid "
        "rgba(16,185,129,0.15); border-radius: 8px;");
    auto *revLayout = new QHBoxLayout(revCard);
    revLayout->setContentsMargins(16, 10, 16, 10);

    auto *revLabel = new QLabel("CA total (payees)", revCard);
    revLabel->setStyleSheet("color: #71717a; font-size: 12px;");
    revLayout->addWidget(revLabel);
    revLayout->addStretch();

    auto *revValue =
        new QLabel(QString("%1 EUR").arg(totalRevenue, 0, 'f', 2), revCard);
    revValue->setStyleSheet(
        "color: #6ee7b7; font-size: 16px; font-weight: 700;");
    revLayout->addWidget(revValue);

    mainLayout->addWidget(revCard);
  }

  auto *closeBtn = new QPushButton("Fermer", &dlg);
  closeBtn->setFixedHeight(40);
  closeBtn->setCursor(Qt::PointingHandCursor);
  closeBtn->setStyleSheet(
      "QPushButton { background: #27272a; color: #a1a1aa; border: none; "
      "border-radius: 8px; padding: 0 24px; font-weight: 500; }"
      "QPushButton:hover { background: #3f3f46; color: white; }");
  connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
  mainLayout->addWidget(closeBtn);

  dlg.exec();
}

void ClientsPage::onImportCsv() {
  QString path =
      QFileDialog::getOpenFileName(this, "Importer clients", "", "CSV (*.csv)");
  if (path.isEmpty())
    return;

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QMessageBox::warning(this, "Erreur", "Impossible d'ouvrir le fichier.");
    return;
  }

  QTextStream in(&file);
  QString headerLine = in.readLine(); // Skip header
  Q_UNUSED(headerLine);

  int imported = 0;
  while (!in.atEnd()) {
    QString line = in.readLine().trimmed();
    if (line.isEmpty())
      continue;

    QStringList parts = line.split(';');
    if (parts.isEmpty())
      continue;

    Client c;
    c.name = parts.value(0).trimmed();
    if (c.name.isEmpty())
      continue;
    c.email = parts.value(1).trimmed();
    c.phone = parts.value(2).trimmed();
    c.company = parts.value(3).trimmed();
    c.address = parts.value(4).trimmed();
    c.notes = parts.value(5).trimmed();
    // tags at index 6 if present
    c.tags = parts.value(6).trimmed();

    ClientModel::create(c);
    ++imported;
  }
  file.close();

  ActivityLog::log("import", "client",
                   QString("Import CSV: %1 clients").arg(imported));

  QMessageBox::information(
      this, "Import termine",
      QString("%1 client(s) importe(s) avec succes.").arg(imported));

  refresh();
}

void ClientsPage::onExportCsv() {
  QString path = QFileDialog::getSaveFileName(this, "Exporter clients",
                                              "clients.csv", "CSV (*.csv)");
  if (path.isEmpty())
    return;

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    return;

  QTextStream out(&file);
  out << "Nom;Email;Telephone;Societe;Adresse;Notes;Tags;Date creation\n";

  auto clients = ClientModel::all();
  for (const auto &c : clients) {
    out << c.name << ";" << c.email << ";" << c.phone << ";" << c.company << ";"
        << c.address << ";" << c.notes << ";" << c.tags << ";"
        << c.createdAt.toString("yyyy-MM-dd") << "\n";
  }

  file.close();
  ActivityLog::log("export", "client", "Export CSV clients");
}

void ClientsPage::onBatchDelete() {
  auto selected = m_table->selectionModel()->selectedRows();
  if (selected.isEmpty())
    return;

  int count = selected.size();
  auto reply = QMessageBox::warning(
      this, "Supprimer",
      QString("Supprimer %1 client(s) selectionne(s) ?").arg(count),
      QMessageBox::Yes | QMessageBox::No);
  if (reply != QMessageBox::Yes)
    return;

  for (const auto &idx : selected) {
    if (!m_table->item(idx.row(), 1))
      continue;
    int id = m_table->item(idx.row(), 1)->data(Qt::UserRole).toInt();
    QString name = m_table->item(idx.row(), 1)->text();
    ClientModel::remove(id);
    ActivityLog::log("delete", "client", "Client: " + name);
  }
  refresh();
}
