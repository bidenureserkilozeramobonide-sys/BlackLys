#include "InvoicesPage.h"
#include "../database/ActivityLog.h"
#include "../widgets/AnimatedCounter.h"
#include "../widgets/ToastWidget.h"
#include "widgets/PageHeader.h"

#include "database/ClientModel.h"
#include "database/Database.h"
#include "database/InvoiceModel.h"
#include "database/MissionModel.h"

#include <QApplication>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPdfWriter>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>

InvoicesPage::InvoicesPage(QWidget *parent) : QWidget(parent) {
  setObjectName("page");
  setupUi();
  refresh();
}

// ════════════════════════════════════════════════
//  Stat Card Helper
// ════════════════════════════════════════════════

QWidget *InvoicesPage::createStatCard(const QString &icon, const QString &value,
                                      const QString &label,
                                      const QString &color) {
  auto *card = new QWidget(this);
  card->setObjectName("invoiceStatCard");
  card->setMinimumHeight(90);
  card->setMaximumHeight(100);
  card->setStyleSheet(
      QString("#invoiceStatCard { background: #0f0f11; "
              "border: 1px solid rgba(255,255,255,0.04); "
              "border-radius: 12px; border-left: 3px solid %1; }")
          .arg(color));

  auto *layout = new QHBoxLayout(card);
  layout->setContentsMargins(18, 14, 18, 14);
  layout->setSpacing(14);

  // Icon
  auto *iconLabel = new QLabel(icon, card);
  QFont iconFont("Segoe MDL2 Assets", 20);
  iconLabel->setFont(iconFont);
  iconLabel->setStyleSheet(
      QString("color: %1; background: transparent;").arg(color));
  iconLabel->setFixedSize(40, 40);
  iconLabel->setAlignment(Qt::AlignCenter);
  layout->addWidget(iconLabel);

  // Text
  auto *textLayout = new QVBoxLayout();
  textLayout->setSpacing(2);

  auto *valueLabel = new AnimatedCounter(card);
  valueLabel->setText(value);
  valueLabel->setObjectName("statValue");
  QFont valueFont("Segoe UI", 22);
  valueFont.setWeight(QFont::Bold);
  valueLabel->setFont(valueFont);
  valueLabel->setStyleSheet("color: #fafafa; background: transparent;");
  textLayout->addWidget(valueLabel);

  auto *labelWidget = new QLabel(label, card);
  labelWidget->setStyleSheet(
      "color: #71717a; font-size: 11px; background: transparent; "
      "text-transform: uppercase; letter-spacing: 1px; font-weight: 600;");
  textLayout->addWidget(labelWidget);

  layout->addLayout(textLayout, 1);

  return card;
}

// ════════════════════════════════════════════════
//  Setup UI
// ════════════════════════════════════════════════

void InvoicesPage::setupUi() {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // Header
  auto *header = new PageHeader("Facturation", "Devis et factures", this);
  mainLayout->addWidget(header);

  // Scroll Area for content
  m_scrollArea = new QScrollArea(this);
  m_scrollArea->setWidgetResizable(true);
  m_scrollArea->setFrameShape(QFrame::NoFrame);
  m_scrollArea->setStyleSheet(
      "QScrollArea { background: transparent; border: none; }"
      "QScrollBar:vertical { background: #09090b; width: 8px; "
      "border-radius: 4px; }"
      "QScrollBar::handle:vertical { background: #27272a; min-height: 30px; "
      "border-radius: 4px; }"
      "QScrollBar::handle:vertical:hover { background: #3f3f46; }"
      "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
      "height: 0; }");

  auto *scrollContent = new QWidget(m_scrollArea);
  auto *contentLayout = new QVBoxLayout(scrollContent);
  contentLayout->setContentsMargins(32, 24, 32, 24);
  contentLayout->setSpacing(20);

  // ── Stat Cards Row ──
  auto *statsRow = new QWidget(scrollContent);
  auto *statsLayout = new QHBoxLayout(statsRow);
  statsLayout->setContentsMargins(0, 0, 0, 0);
  statsLayout->setSpacing(16);

  // Total invoices
  auto *totalCard =
      createStatCard(QString(QChar(0xE8C7)), "0", "Total Factures", "#f59e0b");
  m_statTotal = totalCard->findChild<AnimatedCounter *>("statValue");
  statsLayout->addWidget(totalCard);

  // Paid
  auto *paidCard =
      createStatCard(QString(QChar(0xE73E)), "0", "Payees", "#10b981");
  m_statPaid = paidCard->findChild<AnimatedCounter *>("statValue");
  statsLayout->addWidget(paidCard);

  // Pending
  auto *pendingCard =
      createStatCard(QString(QChar(0xE823)), "0", "En attente", "#3b82f6");
  m_statPending = pendingCard->findChild<AnimatedCounter *>("statValue");
  statsLayout->addWidget(pendingCard);

  // Overdue
  auto *overdueCard =
      createStatCard(QString(QChar(0xEA39)), "0", "En retard", "#ef4444");
  m_statOverdue = overdueCard->findChild<AnimatedCounter *>("statValue");
  statsLayout->addWidget(overdueCard);

  contentLayout->addWidget(statsRow);

  // ── Date range filter ──
  auto *dateRow = new QWidget(scrollContent);
  auto *dateRowLayout = new QHBoxLayout(dateRow);
  dateRowLayout->setContentsMargins(0, 0, 0, 0);
  dateRowLayout->setSpacing(10);

  auto *dateLabel = new QLabel("Periode :", dateRow);
  dateLabel->setStyleSheet("color: #71717a; font-size: 12px; font-weight: 600; "
                           "background: transparent;");
  dateRowLayout->addWidget(dateLabel);

  m_dateFrom = new QDateEdit(dateRow);
  m_dateFrom->setCalendarPopup(true);
  m_dateFrom->setDisplayFormat("dd/MM/yyyy");
  m_dateFrom->setDate(QDate::currentDate().addMonths(-3));
  m_dateFrom->setFixedHeight(32);
  m_dateFrom->setFixedWidth(130);
  m_dateFrom->setStyleSheet(
      "QDateEdit { background: #18181b; color: #e4e4e7; "
      "border: 1px solid #27272a; border-radius: 6px; padding: 4px 8px; }"
      "QDateEdit:focus { border-color: #f59e0b; }");
  connect(m_dateFrom, &QDateEdit::dateChanged, this,
          &InvoicesPage::onFilterChanged);
  dateRowLayout->addWidget(m_dateFrom);

  auto *toLabel = new QLabel(QString::fromUtf8("\xe2\x86\x92"), dateRow);
  toLabel->setStyleSheet("color: #52525b; background: transparent;");
  dateRowLayout->addWidget(toLabel);

  m_dateTo = new QDateEdit(dateRow);
  m_dateTo->setCalendarPopup(true);
  m_dateTo->setDisplayFormat("dd/MM/yyyy");
  m_dateTo->setDate(QDate::currentDate());
  m_dateTo->setFixedHeight(32);
  m_dateTo->setFixedWidth(130);
  m_dateTo->setStyleSheet(
      "QDateEdit { background: #18181b; color: #e4e4e7; "
      "border: 1px solid #27272a; border-radius: 6px; padding: 4px 8px; }"
      "QDateEdit:focus { border-color: #f59e0b; }");
  connect(m_dateTo, &QDateEdit::dateChanged, this,
          &InvoicesPage::onFilterChanged);
  dateRowLayout->addWidget(m_dateTo);

  auto *resetDateBtn = new QPushButton("Tout", dateRow);
  resetDateBtn->setFixedHeight(32);
  resetDateBtn->setCursor(Qt::PointingHandCursor);
  resetDateBtn->setStyleSheet(
      "QPushButton { background: #27272a; color: #a1a1aa; border: none; "
      "border-radius: 6px; padding: 0 12px; font-size: 11px; }"
      "QPushButton:hover { background: #3f3f46; color: white; }");
  connect(resetDateBtn, &QPushButton::clicked, this, [this]() {
    m_dateFrom->setDate(QDate(2000, 1, 1));
    m_dateTo->setDate(QDate::currentDate());
    onFilterChanged();
  });
  dateRowLayout->addWidget(resetDateBtn);
  dateRowLayout->addStretch();

  contentLayout->addWidget(dateRow);

  // ── Top bar: filter + search + buttons ──
  auto *topBar = new QWidget(scrollContent);
  auto *topLayout = new QHBoxLayout(topBar);
  topLayout->setContentsMargins(0, 0, 0, 0);
  topLayout->setSpacing(10);

  // Status filter
  m_filterCombo = new QComboBox(topBar);
  m_filterCombo->addItems(
      {"Toutes", "Brouillon", "Envoyee", "Payee", "Annulee", "En retard"});
  m_filterCombo->setFixedHeight(36);
  m_filterCombo->setFixedWidth(160);
  connect(m_filterCombo, &QComboBox::currentIndexChanged, this,
          &InvoicesPage::onFilterChanged);
  topLayout->addWidget(m_filterCombo);

  // Search
  m_searchInput = new QLineEdit(topBar);
  m_searchInput->setPlaceholderText("Rechercher par numero ou client...");
  m_searchInput->setFixedHeight(36);
  m_searchInput->setMaximumWidth(300);
  connect(m_searchInput, &QLineEdit::textChanged, this,
          &InvoicesPage::onFilterChanged);
  topLayout->addWidget(m_searchInput);

  topLayout->addStretch();

  // Add button
  auto *addBtn = new QPushButton("+ Nouvelle facture", topBar);
  addBtn->setFixedHeight(36);
  addBtn->setCursor(Qt::PointingHandCursor);
  addBtn->setStyleSheet(
      "QPushButton { background: #f59e0b; color: white; border: none; "
      "border-radius: 8px; padding: 0 20px; font-weight: 600; font-size: "
      "12px; }"
      "QPushButton:hover { background: #fbbf24; }");
  connect(addBtn, &QPushButton::clicked, this, &InvoicesPage::onAddInvoice);
  topLayout->addWidget(addBtn);

  auto *csvBtn = new QPushButton("Exporter CSV", topBar);
  csvBtn->setFixedHeight(36);
  csvBtn->setCursor(Qt::PointingHandCursor);
  csvBtn->setStyleSheet(
      "QPushButton { background: #27272a; color: #a1a1aa; border: none; "
      "border-radius: 8px; padding: 0 16px; font-weight: 500; font-size: "
      "12px; }"
      "QPushButton:hover { background: #3f3f46; color: #e4e4e7; }");
  connect(csvBtn, &QPushButton::clicked, this, &InvoicesPage::onExportCsv);
  topLayout->addWidget(csvBtn);

  auto *batchDelBtn = new QPushButton(
      QString("  %1  Supprimer selection").arg(QChar(0xE74D)), topBar);
  batchDelBtn->setFixedHeight(36);
  batchDelBtn->setCursor(Qt::PointingHandCursor);
  batchDelBtn->setStyleSheet(
      "QPushButton { background: rgba(239,68,68,0.1); color: #ef4444; "
      "border: 1px solid rgba(239,68,68,0.2); border-radius: 8px; "
      "padding: 0 16px; font-weight: 500; font-size: 12px; }"
      "QPushButton:hover { background: rgba(239,68,68,0.2); }");
  connect(batchDelBtn, &QPushButton::clicked, this,
          &InvoicesPage::onBatchDelete);
  topLayout->addWidget(batchDelBtn);

  contentLayout->addWidget(topBar);

  // ── Table ──
  m_table = new QTableWidget(scrollContent);
  m_table->setColumnCount(7);
  m_table->setHorizontalHeaderLabels(
      {"N\u00b0", "Client", "Date", "Echeance", "Montant TTC", "Statut", ""});
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_table->verticalHeader()->setVisible(false);
  m_table->horizontalHeader()->setStretchLastSection(false);
  m_table->horizontalHeader()->setSectionResizeMode(
      0, QHeaderView::ResizeToContents);
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
  m_table->setAlternatingRowColors(false);
  m_table->setShowGrid(false);
  m_table->setStyleSheet(
      "QTableWidget { background: #18181b; border: 1px solid #27272a; "
      "border-radius: 10px; color: #e4e4e7; }"
      "QTableWidget::item { padding: 8px; border-bottom: 1px solid "
      "rgba(255,255,255,0.03); }"
      "QTableWidget::item:selected { background: rgba(245,158,11,0.08); }"
      "QTableWidget::item:hover { background: rgba(255,255,255,0.03); }"
      "QHeaderView::section { background: #18181b; color: #71717a; "
      "border: none; padding: 10px 8px; font-weight: 600; font-size: 11px; "
      "text-transform: uppercase; letter-spacing: 1px; "
      "border-bottom: 1px solid #27272a; }"
      "QHeaderView::section:hover { color: #e4e4e7; cursor: pointer; }");

  m_table->horizontalHeader()->setSortIndicatorShown(true);
  connect(m_table->horizontalHeader(), &QHeaderView::sectionClicked, this,
          &InvoicesPage::onSortByColumn);
  connect(m_table, &QTableWidget::cellDoubleClicked, this,
          &InvoicesPage::onEditInvoice);
  contentLayout->addWidget(m_table, 1);

  m_scrollArea->setWidget(scrollContent);
  mainLayout->addWidget(m_scrollArea, 1);
}

// ════════════════════════════════════════════════
//  Refresh
// ════════════════════════════════════════════════

void InvoicesPage::refresh() {
  refreshStats();
  populateTable();
}

void InvoicesPage::refreshStats() {
  int total = InvoiceModel::count();
  int paid = InvoiceModel::countByStatus("payee");
  int sent = InvoiceModel::countByStatus("envoyee");
  int draft = InvoiceModel::countByStatus("brouillon");
  int overdue = InvoiceModel::countOverdue();

  if (m_statTotal)
    m_statTotal->setTargetValue(total);
  if (m_statPaid)
    m_statPaid->setTargetValue(paid);
  if (m_statPending)
    m_statPending->setTargetValue(sent + draft);
  if (m_statOverdue)
    m_statOverdue->setTargetValue(overdue);
}

void InvoicesPage::onFilterChanged() { populateTable(); }

// ════════════════════════════════════════════════
//  Populate Table
// ════════════════════════════════════════════════

void InvoicesPage::populateTable() {
  auto allInvoices = InvoiceModel::all();
  QString search =
      m_searchInput ? m_searchInput->text().trimmed().toLower() : "";
  QString filterStatus;
  bool filterOverdue = false;

  if (m_filterCombo && m_filterCombo->currentIndex() > 0) {
    QString selectedText = m_filterCombo->currentText().toLower();
    if (selectedText == "en retard") {
      filterOverdue = true;
    } else {
      filterStatus = selectedText;
    }
  }

  QDate today = QDate::currentDate();

  // Filter
  QList<Invoice> filtered;
  for (const auto &inv : allInvoices) {
    // Date range filter
    if (m_dateFrom && m_dateTo) {
      QDate invDate = QDate::fromString(inv.date, "yyyy-MM-dd");
      if (invDate.isValid()) {
        if (invDate < m_dateFrom->date() || invDate > m_dateTo->date())
          continue;
      }
    }
    // Overdue filter
    if (filterOverdue) {
      QDate due = QDate::fromString(inv.dueDate, "yyyy-MM-dd");
      bool isOverdue = due.isValid() && due < today && inv.status != "payee" &&
                       inv.status != "annulee";
      if (!isOverdue)
        continue;
    }
    // Status filter
    if (!filterStatus.isEmpty() && inv.status != filterStatus)
      continue;
    // Search filter
    if (!search.isEmpty() && !inv.number.toLower().contains(search) &&
        !inv.clientName.toLower().contains(search))
      continue;
    filtered.append(inv);
  }

  // ── Empty state ──
  if (filtered.isEmpty()) {
    m_table->setRowCount(1);
    m_table->setSpan(0, 0, 1, 7);
    m_table->setRowHeight(0, 160);
    auto *emptyWidget = new QWidget();
    auto *emptyLayout = new QVBoxLayout(emptyWidget);
    emptyLayout->setAlignment(Qt::AlignCenter);
    emptyLayout->setSpacing(12);

    auto *emptyIcon = new QLabel(QString(QChar(0xE8C7)), emptyWidget);
    QFont eiFont("Segoe MDL2 Assets", 32);
    emptyIcon->setFont(eiFont);
    emptyIcon->setStyleSheet("color: #27272a; background: transparent;");
    emptyIcon->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyIcon);

    auto *emptyText = new QLabel("Aucune facture", emptyWidget);
    emptyText->setStyleSheet(
        "color: #52525b; font-size: 14px; font-weight: 500; "
        "background: transparent;");
    emptyText->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyText);

    auto *emptyBtn = new QPushButton("+ Creer une facture", emptyWidget);
    emptyBtn->setCursor(Qt::PointingHandCursor);
    emptyBtn->setFixedSize(180, 36);
    emptyBtn->setStyleSheet(
        "QPushButton { background: #f59e0b; color: white; border: none; "
        "border-radius: 8px; font-weight: 600; font-size: 12px; }"
        "QPushButton:hover { background: #fbbf24; }");
    connect(emptyBtn, &QPushButton::clicked, this, &InvoicesPage::onAddInvoice);
    emptyLayout->addWidget(emptyBtn, 0, Qt::AlignCenter);

    m_table->setCellWidget(0, 0, emptyWidget);
    return;
  }

  // ── Populate rows ──
  m_table->clearSpans();
  m_table->setRowCount(filtered.size());
  for (int i = 0; i < filtered.size(); ++i) {
    const auto &inv = filtered[i];
    m_table->setRowHeight(i, 52);

    // Check if overdue
    QDate due = QDate::fromString(inv.dueDate, "yyyy-MM-dd");
    bool isOverdue = due.isValid() && due < today && inv.status != "payee" &&
                     inv.status != "annulee";

    // Number
    auto *numItem = new QTableWidgetItem(inv.number);
    numItem->setData(Qt::UserRole, inv.id);
    numItem->setForeground(QColor("#fbbf24"));
    QFont numFont;
    numFont.setBold(true);
    numItem->setFont(numFont);
    m_table->setItem(i, 0, numItem);

    // Client
    auto *clientItem = new QTableWidgetItem(
        inv.clientName.isEmpty() ? QString::fromUtf8("\xe2\x80\x94")
                                 : inv.clientName);
    m_table->setItem(i, 1, clientItem);

    // Date
    m_table->setItem(i, 2, new QTableWidgetItem(inv.date));

    // Due date — highlight if overdue
    auto *dueItem = new QTableWidgetItem(inv.dueDate);
    if (isOverdue) {
      dueItem->setForeground(QColor("#ef4444"));
      QFont dueFont;
      dueFont.setBold(true);
      dueItem->setFont(dueFont);
    }
    m_table->setItem(i, 3, dueItem);

    // Amount (TTC)
    auto *amountItem =
        new QTableWidgetItem(QString("%1 EUR").arg(inv.total, 0, 'f', 2));
    amountItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QFont amountFont;
    amountFont.setBold(true);
    amountItem->setFont(amountFont);
    if (isOverdue) {
      amountItem->setForeground(QColor("#ef4444"));
    } else if (inv.total > 0) {
      amountItem->setForeground(QColor("#10b981"));
    }
    m_table->setItem(i, 4, amountItem);

    // ── Status Badge (pill-shaped widget) ──
    QString badgeColor, badgeText, badgeBg;
    if (isOverdue) {
      badgeColor = "#ef4444";
      badgeText = "En retard";
      badgeBg = "rgba(239,68,68,0.15)";
    } else if (inv.status == "payee") {
      badgeColor = "#10b981";
      badgeText = "Payee";
      badgeBg = "rgba(16,185,129,0.15)";
    } else if (inv.status == "envoyee") {
      badgeColor = "#f59e0b";
      badgeText = "Envoyee";
      badgeBg = "rgba(245,158,11,0.15)";
    } else if (inv.status == "annulee") {
      badgeColor = "#ef4444";
      badgeText = "Annulee";
      badgeBg = "rgba(239,68,68,0.15)";
    } else {
      badgeColor = "#71717a";
      badgeText = "Brouillon";
      badgeBg = "rgba(113,113,122,0.15)";
    }

    auto *badgeWidget = new QWidget();
    auto *badgeLayout = new QHBoxLayout(badgeWidget);
    badgeLayout->setContentsMargins(8, 4, 8, 4);
    badgeLayout->setAlignment(Qt::AlignCenter);

    auto *badge = new QLabel(badgeText, badgeWidget);
    badge->setStyleSheet(
        QString("background: %1; color: %2; border-radius: 10px; "
                "padding: 3px 10px; font-size: 11px; font-weight: 600;")
            .arg(badgeBg, badgeColor));
    badge->setAlignment(Qt::AlignCenter);
    badge->setFixedHeight(22);
    badgeLayout->addWidget(badge);
    m_table->setCellWidget(i, 5, badgeWidget);

    // ── Actions cell: contextual buttons ──
    auto *actionsWidget = new QWidget();
    auto *actionsLayout = new QHBoxLayout(actionsWidget);
    actionsLayout->setContentsMargins(4, 0, 4, 0);
    actionsLayout->setSpacing(4);

    // Quick status change buttons (contextual)
    if (inv.status == "brouillon") {
      auto *sendBtn = new QPushButton(QString(QChar(0xE724)));
      sendBtn->setToolTip("Marquer comme envoyee");
      sendBtn->setFixedSize(28, 28);
      sendBtn->setCursor(Qt::PointingHandCursor);
      sendBtn->setStyleSheet(
          "QPushButton { background: rgba(59,130,246,0.1); color: #3b82f6; "
          "border: none; border-radius: 6px; font-size: 12px; }"
          "QPushButton:hover { background: rgba(59,130,246,0.2); }");
      connect(sendBtn, &QPushButton::clicked, this,
              [this, i]() { onMarkAsSent(i); });
      actionsLayout->addWidget(sendBtn);
    }

    if (inv.status != "payee" && inv.status != "annulee") {
      auto *paidBtn = new QPushButton(QString(QChar(0xE73E)));
      paidBtn->setToolTip("Marquer comme payee");
      paidBtn->setFixedSize(28, 28);
      paidBtn->setCursor(Qt::PointingHandCursor);
      paidBtn->setStyleSheet(
          "QPushButton { background: rgba(16,185,129,0.1); color: #10b981; "
          "border: none; border-radius: 6px; font-size: 12px; }"
          "QPushButton:hover { background: rgba(16,185,129,0.2); }");
      connect(paidBtn, &QPushButton::clicked, this,
              [this, i]() { onMarkAsPaid(i); });
      actionsLayout->addWidget(paidBtn);
    }

    // Duplicate
    auto *dupBtn = new QPushButton(QString(QChar(0xE8C8)));
    dupBtn->setToolTip("Dupliquer la facture");
    dupBtn->setFixedSize(28, 28);
    dupBtn->setCursor(Qt::PointingHandCursor);
    dupBtn->setStyleSheet(
        "QPushButton { background: #27272a; color: #a1a1aa; border: none; "
        "border-radius: 6px; font-size: 12px; }"
        "QPushButton:hover { background: #3f3f46; color: white; }");
    connect(dupBtn, &QPushButton::clicked, this,
            [this, i]() { onDuplicateInvoice(i); });
    actionsLayout->addWidget(dupBtn);

    // Edit
    auto *editBtn = new QPushButton("Modifier");
    editBtn->setFixedSize(72, 28);
    editBtn->setCursor(Qt::PointingHandCursor);
    editBtn->setStyleSheet(
        "QPushButton { background: #27272a; color: #a1a1aa; border: none; "
        "border-radius: 6px; font-size: 11px; }"
        "QPushButton:hover { background: #3f3f46; color: white; }");
    connect(editBtn, &QPushButton::clicked, this,
            [this, i]() { onEditInvoice(i); });
    actionsLayout->addWidget(editBtn);

    // Delete
    auto *delBtn = new QPushButton("\xE2\x9C\x95"); // ✕
    delBtn->setFixedSize(28, 28);
    delBtn->setCursor(Qt::PointingHandCursor);
    delBtn->setStyleSheet(
        "QPushButton { background: #27272a; color: #ef4444; border: none; "
        "border-radius: 6px; font-size: 12px; }"
        "QPushButton:hover { background: #451a1a; }");
    connect(delBtn, &QPushButton::clicked, this, [this, i]() {
      int id = m_table->item(i, 0)->data(Qt::UserRole).toInt();
      if (QMessageBox::question(this, "Supprimer",
                                "Supprimer cette facture ?") ==
          QMessageBox::Yes) {
        InvoiceModel::remove(id);
        refresh();
      }
    });
    actionsLayout->addWidget(delBtn);

    // PDF export button
    auto *pdfBtn = new QPushButton("PDF");
    pdfBtn->setFixedSize(42, 28);
    pdfBtn->setCursor(Qt::PointingHandCursor);
    pdfBtn->setStyleSheet(
        "QPushButton { background: #18181b; color: #fbbf24; border: none; "
        "border-radius: 6px; font-size: 11px; font-weight: 600; }"
        "QPushButton:hover { background: #27272a; }");
    connect(pdfBtn, &QPushButton::clicked, this, [this, i]() {
      int id = m_table->item(i, 0)->data(Qt::UserRole).toInt();
      exportPdf(id);
    });
    actionsLayout->addWidget(pdfBtn);

    m_table->setCellWidget(i, 6, actionsWidget);

    // ── Row background for overdue ──
    if (isOverdue) {
      QColor overdueRowBg(239, 68, 68, 8); // very subtle red tint
      for (int col = 0; col < 5; ++col) {
        if (auto *item = m_table->item(i, col)) {
          item->setBackground(overdueRowBg);
        }
      }
    }
  }
}

// ════════════════════════════════════════════════
//  Invoice Dialog
// ════════════════════════════════════════════════

static bool showInvoiceDialog(QWidget *parent, Invoice &invoice,
                              bool isNew = true) {
  QDialog dlg(parent);
  dlg.setWindowTitle(isNew ? "Nouvelle facture" : "Modifier la facture");
  dlg.setMinimumWidth(520);
  dlg.setStyleSheet(
      "QDialog { background: #09090b; color: #e4e4e7; }"
      "QLabel { color: #a1a1aa; background: transparent; }"
      "QLineEdit, QComboBox, QDateEdit, QDoubleSpinBox, QTextEdit { "
      "  background: #18181b; color: #e4e4e7; border: 1px solid #27272a; "
      "  border-radius: 6px; padding: 6px; }"
      "QLineEdit:focus, QComboBox:focus, QDateEdit:focus { "
      "  border-color: #f59e0b; }");

  auto *layout = new QVBoxLayout(&dlg);

  auto *form = new QFormLayout();
  form->setSpacing(10);

  // Client dropdown
  auto *clientCombo = new QComboBox(&dlg);
  clientCombo->addItem(
      QString::fromUtf8("\xe2\x80\x94 Aucun client \xe2\x80\x94"), 0);
  auto clients = ClientModel::all();
  for (const auto &c : clients) {
    clientCombo->addItem(c.name, c.id);
  }
  // Select current client
  for (int i = 0; i < clientCombo->count(); ++i) {
    if (clientCombo->itemData(i).toInt() == invoice.clientId) {
      clientCombo->setCurrentIndex(i);
      break;
    }
  }
  form->addRow("Client", clientCombo);

  // Invoice number (auto-generated for new)
  auto *numberEdit =
      new QLineEdit(isNew ? InvoiceModel::nextNumber() : invoice.number, &dlg);
  numberEdit->setReadOnly(!isNew);
  form->addRow("Numero", numberEdit);

  // Date
  auto *dateEdit = new QDateEdit(&dlg);
  dateEdit->setCalendarPopup(true);
  dateEdit->setDisplayFormat("yyyy-MM-dd");
  dateEdit->setDate(invoice.date.isEmpty()
                        ? QDate::currentDate()
                        : QDate::fromString(invoice.date, "yyyy-MM-dd"));
  form->addRow("Date", dateEdit);

  // Due date
  auto *dueDateEdit = new QDateEdit(&dlg);
  dueDateEdit->setCalendarPopup(true);
  dueDateEdit->setDisplayFormat("yyyy-MM-dd");
  dueDateEdit->setDate(invoice.dueDate.isEmpty()
                           ? QDate::currentDate().addDays(30)
                           : QDate::fromString(invoice.dueDate, "yyyy-MM-dd"));
  form->addRow("Echeance", dueDateEdit);

  // Status
  auto *statusCombo = new QComboBox(&dlg);
  statusCombo->addItems({"brouillon", "envoyee", "payee", "annulee"});
  int statusIdx = statusCombo->findText(invoice.status);
  if (statusIdx >= 0)
    statusCombo->setCurrentIndex(statusIdx);
  form->addRow("Statut", statusCombo);

  // Tax rate
  auto *taxSpin = new QDoubleSpinBox(&dlg);
  taxSpin->setRange(0, 100);
  taxSpin->setSuffix(" %");
  taxSpin->setValue(invoice.taxRate);
  form->addRow("TVA", taxSpin);

  // Notes
  auto *notesEdit = new QLineEdit(invoice.notes, &dlg);
  form->addRow("Notes", notesEdit);

  layout->addLayout(form);

  // ── Line Items ──
  auto *itemsLabel = new QLabel("Lignes de facturation", &dlg);
  itemsLabel->setStyleSheet(
      "color: #d97706; font-size: 13px; font-weight: 700; "
      "padding: 12px 0 4px 0; background: transparent;");
  layout->addWidget(itemsLabel);

  auto *itemsTable = new QTableWidget(&dlg);
  itemsTable->setColumnCount(4);
  itemsTable->setHorizontalHeaderLabels(
      {"Description", "Qte", "Prix unit.", "Total"});
  itemsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  itemsTable->horizontalHeader()->setSectionResizeMode(
      1, QHeaderView::ResizeToContents);
  itemsTable->horizontalHeader()->setSectionResizeMode(
      2, QHeaderView::ResizeToContents);
  itemsTable->horizontalHeader()->setSectionResizeMode(
      3, QHeaderView::ResizeToContents);
  itemsTable->verticalHeader()->setVisible(false);
  itemsTable->setFixedHeight(200);
  itemsTable->setStyleSheet(
      "QTableWidget { background: #18181b; border: 1px solid #27272a; "
      "border-radius: 6px; color: #e4e4e7; }"
      "QHeaderView::section { background: #18181b; color: #71717a; "
      "border: none; padding: 6px; font-size: 11px; }");

  // Populate existing items
  if (!isNew) {
    auto items = InvoiceModel::getItems(invoice.id);
    itemsTable->setRowCount(items.size());
    for (int i = 0; i < items.size(); ++i) {
      itemsTable->setItem(i, 0, new QTableWidgetItem(items[i].description));
      itemsTable->setItem(
          i, 1,
          new QTableWidgetItem(QString::number(items[i].quantity, 'f', 1)));
      itemsTable->setItem(
          i, 2,
          new QTableWidgetItem(QString::number(items[i].unitPrice, 'f', 2)));
      itemsTable->setItem(
          i, 3, new QTableWidgetItem(QString::number(items[i].total, 'f', 2)));
      // Store item ID
      itemsTable->item(i, 0)->setData(Qt::UserRole, items[i].id);
    }
  }

  layout->addWidget(itemsTable);

  // Add/remove item buttons
  auto *itemBtns = new QWidget(&dlg);
  auto *itemBtnsLayout = new QHBoxLayout(itemBtns);
  itemBtnsLayout->setContentsMargins(0, 0, 0, 0);

  auto *addItemBtn = new QPushButton("+ Ajouter une ligne", itemBtns);
  addItemBtn->setStyleSheet(
      "QPushButton { background: #27272a; color: #a1a1aa; border: none; "
      "border-radius: 6px; padding: 6px 12px; font-size: 12px; }"
      "QPushButton:hover { background: #3f3f46; color: white; }");
  QObject::connect(addItemBtn, &QPushButton::clicked, [itemsTable]() {
    int row = itemsTable->rowCount();
    itemsTable->setRowCount(row + 1);
    itemsTable->setItem(row, 0, new QTableWidgetItem(""));
    itemsTable->setItem(row, 1, new QTableWidgetItem("1.0"));
    itemsTable->setItem(row, 2, new QTableWidgetItem("0.00"));
    itemsTable->setItem(row, 3, new QTableWidgetItem("0.00"));
  });
  itemBtnsLayout->addWidget(addItemBtn);

  auto *removeItemBtn = new QPushButton("Supprimer la ligne", itemBtns);
  removeItemBtn->setStyleSheet(
      "QPushButton { background: #27272a; color: #ef4444; border: none; "
      "border-radius: 6px; padding: 6px 12px; font-size: 12px; }"
      "QPushButton:hover { background: #451a1a; }");
  QObject::connect(removeItemBtn, &QPushButton::clicked, [itemsTable]() {
    int row = itemsTable->currentRow();
    if (row >= 0)
      itemsTable->removeRow(row);
  });
  itemBtnsLayout->addWidget(removeItemBtn);
  itemBtnsLayout->addStretch();

  layout->addWidget(itemBtns);

  // Buttons
  auto *buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  buttons->button(QDialogButtonBox::Ok)
      ->setStyleSheet(
          "QPushButton { background: #f59e0b; color: white; border: none; "
          "border-radius: 6px; padding: 8px 20px; font-weight: 600; }"
          "QPushButton:hover { background: #fbbf24; }");
  buttons->button(QDialogButtonBox::Cancel)
      ->setStyleSheet(
          "QPushButton { background: #27272a; color: #a1a1aa; border: none; "
          "border-radius: 6px; padding: 8px 20px; }"
          "QPushButton:hover { background: #3f3f46; color: white; }");
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg,
                   &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg,
                   &QDialog::reject);
  layout->addWidget(buttons);

  if (dlg.exec() != QDialog::Accepted)
    return false;

  // Gather data
  invoice.clientId = clientCombo->currentData().toInt();
  invoice.number = numberEdit->text();
  invoice.date = dateEdit->date().toString("yyyy-MM-dd");
  invoice.dueDate = dueDateEdit->date().toString("yyyy-MM-dd");
  invoice.status = statusCombo->currentText();
  invoice.taxRate = taxSpin->value();
  invoice.notes = notesEdit->text();

  // Collect line items
  invoice.items.clear();
  for (int i = 0; i < itemsTable->rowCount(); ++i) {
    InvoiceItem item;
    item.description =
        itemsTable->item(i, 0) ? itemsTable->item(i, 0)->text() : "";
    item.quantity = itemsTable->item(i, 1)
                        ? itemsTable->item(i, 1)->text().toDouble()
                        : 1.0;
    item.unitPrice = itemsTable->item(i, 2)
                         ? itemsTable->item(i, 2)->text().toDouble()
                         : 0.0;
    item.total = item.quantity * item.unitPrice;
    // Preserve item ID if editing
    if (itemsTable->item(i, 0) &&
        itemsTable->item(i, 0)->data(Qt::UserRole).isValid()) {
      item.id = itemsTable->item(i, 0)->data(Qt::UserRole).toInt();
    }
    if (!item.description.isEmpty())
      invoice.items.append(item);
  }

  return true;
}

void InvoicesPage::onAddInvoice() {
  Invoice inv;
  if (!showInvoiceDialog(this, inv, true))
    return;

  int id = InvoiceModel::create(inv);
  if (id > 0) {
    // Add line items
    for (auto &item : inv.items) {
      item.invoiceId = id;
      InvoiceModel::addItem(item);
    }
    InvoiceModel::recalculate(id);
  }
  ActivityLog::log("create", "facture", "Facture: " + inv.number);
  ToastWidget::show(window(), "Facture " + inv.number + " creee",
                    ToastWidget::Success);
  refresh();
}

void InvoicesPage::onEditInvoice(int row) {
  if (row < 0 || !m_table->item(row, 0))
    return;
  int id = m_table->item(row, 0)->data(Qt::UserRole).toInt();
  Invoice inv = InvoiceModel::getById(id);
  if (inv.id == 0)
    return;

  if (!showInvoiceDialog(this, inv, false))
    return;

  inv.taxRate = inv.taxRate; // keep tax rate (already set from dialog)
  InvoiceModel::update(inv);

  // Remove old items and re-add
  auto oldItems = InvoiceModel::getItems(inv.id);
  for (const auto &old : oldItems) {
    InvoiceModel::removeItem(old.id);
  }
  for (auto &item : inv.items) {
    item.invoiceId = inv.id;
    InvoiceModel::addItem(item);
  }
  InvoiceModel::recalculate(inv.id);
  ActivityLog::log("edit", "facture", "Facture: " + inv.number);
  ToastWidget::show(window(), "Facture " + inv.number + " modifiee",
                    ToastWidget::Success);
  refresh();
}

void InvoicesPage::onDeleteInvoice() {
  int row = m_table->currentRow();
  if (row < 0 || !m_table->item(row, 0))
    return;
  int id = m_table->item(row, 0)->data(Qt::UserRole).toInt();
  if (QMessageBox::question(this, "Supprimer", "Supprimer cette facture ?") ==
      QMessageBox::Yes) {
    InvoiceModel::remove(id);
    refresh();
  }
}

// ════════════════════════════════════════════════
//  Quick Actions
// ════════════════════════════════════════════════

void InvoicesPage::onMarkAsPaid(int row) {
  if (row < 0 || !m_table->item(row, 0))
    return;
  int id = m_table->item(row, 0)->data(Qt::UserRole).toInt();
  Invoice inv = InvoiceModel::getById(id);
  if (inv.id == 0)
    return;

  inv.status = "payee";
  InvoiceModel::update(inv);
  ActivityLog::log("edit", "facture",
                   "Facture " + inv.number + " marquee payee");
  ToastWidget::show(window(), "Facture " + inv.number + " marquee payee",
                    ToastWidget::Success);
  refresh();
}

void InvoicesPage::onMarkAsSent(int row) {
  if (row < 0 || !m_table->item(row, 0))
    return;
  int id = m_table->item(row, 0)->data(Qt::UserRole).toInt();
  Invoice inv = InvoiceModel::getById(id);
  if (inv.id == 0)
    return;

  inv.status = "envoyee";
  InvoiceModel::update(inv);
  ActivityLog::log("edit", "facture",
                   "Facture " + inv.number + " marquee envoyee");
  ToastWidget::show(window(), "Facture " + inv.number + " marquee envoyee",
                    ToastWidget::Info);
  refresh();
}

void InvoicesPage::onDuplicateInvoice(int row) {
  if (row < 0 || !m_table->item(row, 0))
    return;
  int id = m_table->item(row, 0)->data(Qt::UserRole).toInt();
  Invoice src = InvoiceModel::getById(id);
  if (src.id == 0)
    return;

  // Create a duplicate with new number and reset status
  Invoice dup = src;
  dup.id = 0;
  dup.number = InvoiceModel::nextNumber();
  dup.status = "brouillon";
  dup.date = QDate::currentDate().toString("yyyy-MM-dd");
  dup.dueDate = QDate::currentDate().addDays(30).toString("yyyy-MM-dd");

  int newId = InvoiceModel::create(dup);
  if (newId > 0) {
    // Duplicate line items
    auto items = InvoiceModel::getItems(src.id);
    for (auto &item : items) {
      item.id = 0;
      item.invoiceId = newId;
      InvoiceModel::addItem(item);
    }
    InvoiceModel::recalculate(newId);
  }
  ActivityLog::log("create", "facture",
                   "Facture " + dup.number + " (copie de " + src.number + ")");
  ToastWidget::show(window(), "Facture dupliquee: " + dup.number,
                    ToastWidget::Success);
  refresh();
}

// ════════════════════════════════════════════════
//  PDF Export
// ════════════════════════════════════════════════

static QString pdfGetSetting(const QString &key, const QString &def = "") {
  QSqlQuery q(Database::instance().db());
  q.prepare("SELECT value FROM settings WHERE key = ?");
  q.addBindValue(key);
  q.exec();
  if (q.next())
    return q.value(0).toString();
  return def;
}

void InvoicesPage::exportPdf(int invoiceId) {
  Invoice inv = InvoiceModel::getById(invoiceId);
  if (inv.id == 0)
    return;

  auto items = InvoiceModel::getItems(inv.id);

  // Choose save path
  QString defaultName = QString("Facture_%1.pdf").arg(inv.number);
  QString path = QFileDialog::getSaveFileName(this, "Exporter en PDF",
                                              defaultName, "PDF (*.pdf)");
  if (path.isEmpty())
    return;

  QPdfWriter writer(path);
  writer.setPageSize(QPageSize::A4);
  writer.setResolution(300);
  writer.setPageMargins(QMarginsF(20, 20, 20, 20), QPageLayout::Millimeter);

  QPainter painter(&writer);
  if (!painter.isActive()) {
    QMessageBox::warning(this, "Erreur", "Impossible de creer le PDF.");
    return;
  }

  int pageW = writer.width();
  int y = 0;

  // ── Company header ──
  QString companyName = pdfGetSetting("company_name", "BlackLys Studio");
  QString companyAddr = pdfGetSetting("company_address");
  QString companyPhone = pdfGetSetting("company_phone");
  QString companyEmail = pdfGetSetting("company_email");
  QString companySiret = pdfGetSetting("company_siret");

  QFont titleFont("Segoe UI", 18, QFont::Bold);
  QFont headerFont("Segoe UI", 10, QFont::Bold);
  QFont normalFont("Segoe UI", 9);
  QFont smallFont("Segoe UI", 7);

  // Company name
  painter.setFont(titleFont);
  painter.setPen(QColor("#18181b"));
  painter.drawText(0, y, pageW / 2, 300, Qt::AlignLeft | Qt::AlignTop,
                   companyName);

  // Invoice title
  painter.drawText(pageW / 2, y, pageW / 2, 300, Qt::AlignRight | Qt::AlignTop,
                   QString("FACTURE %1").arg(inv.number));
  y += 350;

  // Company details
  painter.setFont(smallFont);
  painter.setPen(QColor("#71717a"));
  QStringList companyLines;
  if (!companyAddr.isEmpty())
    companyLines << companyAddr;
  if (!companyPhone.isEmpty())
    companyLines << companyPhone;
  if (!companyEmail.isEmpty())
    companyLines << companyEmail;
  if (!companySiret.isEmpty())
    companyLines << "SIRET: " + companySiret;
  for (const auto &line : companyLines) {
    painter.drawText(0, y, pageW, 120, Qt::AlignLeft, line);
    y += 130;
  }
  y += 100;

  // ── Client + dates block ──
  painter.setFont(headerFont);
  painter.setPen(QColor("#18181b"));
  painter.drawText(0, y, pageW / 2, 200, Qt::AlignLeft, "CLIENT");
  painter.drawText(pageW / 2, y, pageW / 2, 200, Qt::AlignRight,
                   QString("Date: %1").arg(inv.date));
  y += 200;

  painter.setFont(normalFont);
  painter.setPen(QColor("#3f3f46"));
  painter.drawText(0, y, pageW / 2, 160, Qt::AlignLeft, inv.clientName);
  painter.drawText(pageW / 2, y, pageW / 2, 160, Qt::AlignRight,
                   QString("Echeance: %1").arg(inv.dueDate));
  y += 200;

  painter.drawText(pageW / 2, y, pageW / 2, 160, Qt::AlignRight,
                   QString("Statut: %1").arg(inv.status));
  y += 300;

  // ── Items table header ──
  int colDesc = pageW * 50 / 100;
  int colQty = pageW * 12 / 100;
  int colUnit = pageW * 19 / 100;
  int colTotal = pageW * 19 / 100;
  int rowH = 180;

  // Header background
  painter.fillRect(0, y, pageW, rowH, QColor("#f4f4f5"));
  painter.setFont(headerFont);
  painter.setPen(QColor("#18181b"));
  int x = 40;
  painter.drawText(x, y, colDesc, rowH, Qt::AlignLeft | Qt::AlignVCenter,
                   "Description");
  x += colDesc;
  painter.drawText(x, y, colQty, rowH, Qt::AlignCenter, "Qte");
  x += colQty;
  painter.drawText(x, y, colUnit, rowH, Qt::AlignRight | Qt::AlignVCenter,
                   "Prix unit.");
  x += colUnit;
  painter.drawText(x, y, colTotal - 40, rowH, Qt::AlignRight | Qt::AlignVCenter,
                   "Total");
  y += rowH;

  // ── Items rows ──
  painter.setFont(normalFont);
  double subtotal = 0;
  for (const auto &item : items) {
    painter.setPen(QColor("#e4e4e7"));
    painter.drawLine(0, y, pageW, y);

    painter.setPen(QColor("#3f3f46"));
    x = 40;
    painter.drawText(x, y, colDesc, rowH, Qt::AlignLeft | Qt::AlignVCenter,
                     item.description);
    x += colDesc;
    painter.drawText(x, y, colQty, rowH, Qt::AlignCenter,
                     QString::number(item.quantity, 'f', 1));
    x += colQty;
    painter.drawText(x, y, colUnit, rowH, Qt::AlignRight | Qt::AlignVCenter,
                     QString("%1 EUR").arg(item.unitPrice, 0, 'f', 2));
    x += colUnit;
    painter.drawText(x, y, colTotal - 40, rowH,
                     Qt::AlignRight | Qt::AlignVCenter,
                     QString("%1 EUR").arg(item.total, 0, 'f', 2));
    subtotal += item.total;
    y += rowH;
  }

  // Bottom line
  painter.setPen(QColor("#18181b"));
  painter.drawLine(0, y, pageW, y);
  y += 100;

  // ── Totals ──
  double tva = subtotal * inv.taxRate / 100.0;
  double ttc = subtotal + tva;

  int totalsX = pageW * 60 / 100;
  int totalsW = pageW - totalsX;

  painter.setFont(normalFont);
  painter.setPen(QColor("#3f3f46"));
  painter.drawText(totalsX, y, totalsW / 2, rowH, Qt::AlignLeft,
                   "Sous-total HT");
  painter.drawText(totalsX + totalsW / 2, y, totalsW / 2, rowH, Qt::AlignRight,
                   QString("%1 EUR").arg(subtotal, 0, 'f', 2));
  y += rowH;

  painter.drawText(totalsX, y, totalsW / 2, rowH, Qt::AlignLeft,
                   QString("TVA %1%").arg(inv.taxRate, 0, 'f', 1));
  painter.drawText(totalsX + totalsW / 2, y, totalsW / 2, rowH, Qt::AlignRight,
                   QString("%1 EUR").arg(tva, 0, 'f', 2));
  y += rowH;

  painter.setFont(headerFont);
  painter.setPen(QColor("#18181b"));
  painter.fillRect(totalsX - 20, y - 10, totalsW + 40, rowH + 20,
                   QColor("#f4f4f5"));
  painter.drawText(totalsX, y, totalsW / 2, rowH, Qt::AlignLeft, "Total TTC");
  painter.drawText(totalsX + totalsW / 2, y, totalsW / 2, rowH, Qt::AlignRight,
                   QString("%1 EUR").arg(ttc, 0, 'f', 2));
  y += rowH + 200;

  // ── Notes ──
  if (!inv.notes.isEmpty()) {
    painter.setFont(smallFont);
    painter.setPen(QColor("#71717a"));
    painter.drawText(0, y, pageW, 300, Qt::AlignLeft | Qt::TextWordWrap,
                     "Notes: " + inv.notes);
    y += 350;
  }

  // ── Footer ──
  painter.setFont(smallFont);
  painter.setPen(QColor("#a1a1aa"));
  int footerY = writer.height() - 200;
  painter.drawText(0, footerY, pageW, 200, Qt::AlignCenter,
                   QString("%1 — Facture %2 — Generee par BlackLys")
                       .arg(companyName, inv.number));

  painter.end();

  // Open the PDF
  QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void InvoicesPage::onExportCsv() {
  QString path = QFileDialog::getSaveFileName(this, "Exporter factures",
                                              "factures.csv", "CSV (*.csv)");
  if (path.isEmpty())
    return;

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    return;

  QTextStream out(&file);
  out << "Numero;Client;Date;Echeance;Montant TTC;Statut\n";

  auto invoices = InvoiceModel::all();
  for (const auto &inv : invoices) {
    out << inv.number << ";" << inv.clientName << ";" << inv.date << ";"
        << inv.dueDate << ";" << inv.total << ";" << inv.status << "\n";
  }

  file.close();
}

void InvoicesPage::onBatchDelete() {
  auto selected = m_table->selectionModel()->selectedRows();
  if (selected.isEmpty())
    return;

  int count = selected.size();
  auto reply = QMessageBox::warning(
      this, "Supprimer",
      QString("Supprimer %1 facture(s) selectionnee(s) ?").arg(count),
      QMessageBox::Yes | QMessageBox::No);
  if (reply != QMessageBox::Yes)
    return;

  for (const auto &idx : selected) {
    int id = m_table->item(idx.row(), 0)->data(Qt::UserRole).toInt();
    QString num = m_table->item(idx.row(), 0)->text();
    InvoiceModel::remove(id);
    ActivityLog::log("delete", "facture", "Facture " + num);
  }
  ToastWidget::show(window(), QString("%1 facture(s) supprimee(s)").arg(count),
                    ToastWidget::Error);
  refresh();
}

// ════════════════════════════════════════════════
//  Column Sorting
// ════════════════════════════════════════════════

void InvoicesPage::onSortByColumn(int column) {
  // Only sort meaningful columns: 0=N°, 1=Client, 2=Date, 3=Echeance, 4=Montant
  if (column > 4)
    return;

  if (m_sortColumn == column) {
    m_sortAscending = !m_sortAscending;
  } else {
    m_sortColumn = column;
    m_sortAscending = true;
  }

  m_table->sortItems(column, m_sortAscending ? Qt::AscendingOrder
                                             : Qt::DescendingOrder);
}
