#include "MissionsPage.h"
#include "../database/ActivityLog.h"
#include "../database/ClientModel.h"
#include "../database/GalleryModel.h"
#include "../database/MissionModel.h"
#include "../widgets/MissionCalendarWidget.h"
#include "../widgets/PageHeader.h"

#include <QAction>
#include <QComboBox>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFileDialog>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPdfWriter>
#include <QPushButton>
#include <QShortcut>
#include <QTextEdit>
#include <QTextStream>
#include <QTimeEdit>
#include <QVBoxLayout>

// ════════════════════════════════════════════════
//  Stat Card Helper
// ════════════════════════════════════════════════

QWidget *MissionsPage::createStatCard(const QString &icon, const QString &value,
                                      const QString &label,
                                      const QString &color) {
  auto *card = new QWidget(this);
  card->setObjectName("missionStatCard");
  card->setMinimumHeight(90);
  card->setMaximumHeight(100);
  card->setCursor(Qt::PointingHandCursor);
  card->setStyleSheet(
      QString("#missionStatCard { background: #0f0f11; "
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

  auto *valueLabel = new QLabel(value, card);
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
//  Event Filter (stat card clicks)
// ════════════════════════════════════════════════

bool MissionsPage::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::MouseButtonPress) {
    if (obj == m_cardPlanned) {
      m_filterCombo->setCurrentIndex(1); // Planifiee
      return true;
    } else if (obj == m_cardInProgress) {
      m_filterCombo->setCurrentIndex(2); // En cours
      return true;
    } else if (obj == m_cardDone) {
      m_filterCombo->setCurrentIndex(3); // Terminee
      return true;
    } else if (obj == m_cardCancelled) {
      m_filterCombo->setCurrentIndex(4); // Annulee
      return true;
    }
  }
  return QWidget::eventFilter(obj, event);
}

// ════════════════════════════════════════════════
//  Constructor
// ════════════════════════════════════════════════

MissionsPage::MissionsPage(QWidget *parent) : QWidget(parent) {
  setObjectName("page");
  setupUi();
  setupShortcuts();
  refresh();
}

// ════════════════════════════════════════════════
//  Setup UI
// ════════════════════════════════════════════════

void MissionsPage::setupUi() {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  m_header =
      new PageHeader("Missions", "Planification de vos shootings photo", this);
  mainLayout->addWidget(m_header);

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

  m_cardPlanned =
      createStatCard(QString(QChar(0xE787)), "0", "Planifiees", "#f59e0b");
  m_statPlanned = m_cardPlanned->findChild<QLabel *>("statValue");
  m_cardPlanned->installEventFilter(this);
  statsLayout->addWidget(m_cardPlanned);

  m_cardInProgress =
      createStatCard(QString(QChar(0xE768)), "0", "En cours", "#3b82f6");
  m_statInProgress = m_cardInProgress->findChild<QLabel *>("statValue");
  m_cardInProgress->installEventFilter(this);
  statsLayout->addWidget(m_cardInProgress);

  m_cardDone =
      createStatCard(QString(QChar(0xE73E)), "0", "Terminees", "#10b981");
  m_statDone = m_cardDone->findChild<QLabel *>("statValue");
  m_cardDone->installEventFilter(this);
  statsLayout->addWidget(m_cardDone);

  m_cardCancelled =
      createStatCard(QString(QChar(0xEA39)), "0", "Annulees", "#ef4444");
  m_statCancelled = m_cardCancelled->findChild<QLabel *>("statValue");
  m_cardCancelled->installEventFilter(this);
  statsLayout->addWidget(m_cardCancelled);

  contentLayout->addWidget(statsRow);

  // ── Toolbar ──
  auto *toolbar = new QWidget(scrollContent);
  auto *toolbarLayout = new QHBoxLayout(toolbar);
  toolbarLayout->setContentsMargins(0, 0, 0, 0);
  toolbarLayout->setSpacing(10);

  // Search input
  m_searchInput = new QLineEdit(toolbar);
  m_searchInput->setPlaceholderText(
      QString("  %1  Rechercher une mission...").arg(QChar(0xE721)));
  m_searchInput->setFixedHeight(36);
  m_searchInput->setMinimumWidth(280);
  connect(m_searchInput, &QLineEdit::textChanged, this,
          &MissionsPage::onSearch);
  toolbarLayout->addWidget(m_searchInput);

  // Status filter combo
  m_filterCombo = new QComboBox(toolbar);
  m_filterCombo->addItems(
      {"Tous les statuts", "Planifiee", "En cours", "Terminee", "Annulee"});
  m_filterCombo->setFixedHeight(36);
  m_filterCombo->setFixedWidth(170);
  connect(m_filterCombo, &QComboBox::currentIndexChanged, this,
          &MissionsPage::onFilterChanged);
  toolbarLayout->addWidget(m_filterCombo);

  toolbarLayout->addStretch();

  auto *addBtn = new QPushButton("+ Nouvelle mission", toolbar);
  addBtn->setFixedHeight(36);
  addBtn->setCursor(Qt::PointingHandCursor);
  addBtn->setStyleSheet(
      "QPushButton { background: #f59e0b; color: white; border: none; "
      "border-radius: 8px; padding: 0 20px; font-weight: 600; font-size: "
      "12px; }"
      "QPushButton:hover { background: #fbbf24; }");
  connect(addBtn, &QPushButton::clicked, this, &MissionsPage::onAddMission);
  toolbarLayout->addWidget(addBtn);

  auto *csvBtn = new QPushButton("Exporter CSV", toolbar);
  csvBtn->setFixedHeight(36);
  csvBtn->setCursor(Qt::PointingHandCursor);
  csvBtn->setStyleSheet(
      "QPushButton { background: #27272a; color: #a1a1aa; border: none; "
      "border-radius: 8px; padding: 0 16px; font-weight: 500; font-size: "
      "12px; }"
      "QPushButton:hover { background: #3f3f46; color: #e4e4e7; }");
  connect(csvBtn, &QPushButton::clicked, this, &MissionsPage::onExportCsv);
  toolbarLayout->addWidget(csvBtn);

  auto *batchDeleteBtn = new QPushButton(
      QString("  %1  Supprimer selection").arg(QChar(0xE74D)), toolbar);
  batchDeleteBtn->setFixedHeight(36);
  batchDeleteBtn->setCursor(Qt::PointingHandCursor);
  batchDeleteBtn->setStyleSheet(
      "QPushButton { background: rgba(239,68,68,0.1); color: #ef4444; "
      "border: 1px solid rgba(239,68,68,0.2); border-radius: 8px; "
      "padding: 0 16px; font-weight: 500; font-size: 12px; }"
      "QPushButton:hover { background: rgba(239,68,68,0.2); }");
  connect(batchDeleteBtn, &QPushButton::clicked, this,
          &MissionsPage::onBatchDelete);
  toolbarLayout->addWidget(batchDeleteBtn);

  contentLayout->addWidget(toolbar);

  // ── Date filter indicator (hidden by default) ──
  m_dateFilterRow = new QWidget(scrollContent);
  auto *dateFilterLayout = new QHBoxLayout(m_dateFilterRow);
  dateFilterLayout->setContentsMargins(0, 0, 0, 0);
  dateFilterLayout->setSpacing(8);

  m_dateFilterLabel = new QLabel("", m_dateFilterRow);
  m_dateFilterLabel->setStyleSheet(
      "background: rgba(245,158,11,0.12); color: #fcd34d; "
      "border-radius: 6px; padding: 4px 12px; "
      "font-size: 12px; font-weight: 500;");
  dateFilterLayout->addWidget(m_dateFilterLabel);

  auto *resetBtn =
      new QPushButton(QString("%1").arg(QChar(0x2715)), m_dateFilterRow);
  resetBtn->setFixedSize(28, 28);
  resetBtn->setCursor(Qt::PointingHandCursor);
  resetBtn->setStyleSheet(
      "QPushButton { background: rgba(239,68,68,0.15); color: #ef4444; "
      "border: none; border-radius: 14px; font-size: 12px; }"
      "QPushButton:hover { background: rgba(239,68,68,0.3); }");
  connect(resetBtn, &QPushButton::clicked, this,
          &MissionsPage::onResetDateFilter);
  dateFilterLayout->addWidget(resetBtn);

  dateFilterLayout->addStretch();
  m_dateFilterRow->hide();

  contentLayout->addWidget(m_dateFilterRow);

  // ── Calendar ──
  m_calendar = new MissionCalendarWidget(scrollContent);
  connect(m_calendar, &MissionCalendarWidget::dayClicked, this,
          &MissionsPage::onCalendarDayClicked);
  contentLayout->addWidget(m_calendar);

  // ── Table ──
  m_table = new QTableWidget(scrollContent);
  m_table->setColumnCount(8);
  m_table->setHorizontalHeaderLabels(
      {"Titre", "Client", "Date", "Heure", "Type", "Statut", "Photos", ""});
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_table->verticalHeader()->setVisible(false);
  m_table->horizontalHeader()->setStretchLastSection(false);
  m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_table->horizontalHeader()->setSectionResizeMode(
      1, QHeaderView::ResizeToContents);
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
  m_table->setAlternatingRowColors(false);
  m_table->setShowGrid(false);
  m_table->setSortingEnabled(true);
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
      "border-bottom: 1px solid #27272a; }");

  // Context menu
  m_table->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_table, &QTableWidget::customContextMenuRequested, this,
          &MissionsPage::onTableContextMenu);

  connect(m_table, &QTableWidget::cellDoubleClicked, this,
          &MissionsPage::onEditMission);
  contentLayout->addWidget(m_table, 1);

  // ── Empty state ──
  m_emptyState = new QWidget(scrollContent);
  m_emptyState->setMinimumHeight(180);
  auto *emptyLayout = new QVBoxLayout(m_emptyState);
  emptyLayout->setAlignment(Qt::AlignCenter);

  auto *emptyIcon = new QLabel(QString(QChar(0xE787)), m_emptyState);
  QFont emptyIconFont("Segoe MDL2 Assets", 36);
  emptyIcon->setFont(emptyIconFont);
  emptyIcon->setStyleSheet("color: #27272a; background: transparent;");
  emptyIcon->setAlignment(Qt::AlignCenter);
  emptyLayout->addWidget(emptyIcon);

  auto *emptyText = new QLabel("Aucune mission trouvee", m_emptyState);
  emptyText->setStyleSheet(
      "color: #52525b; font-size: 14px; background: transparent;");
  emptyText->setAlignment(Qt::AlignCenter);
  emptyLayout->addWidget(emptyText);

  auto *emptyHint = new QLabel(
      "Creez une nouvelle mission ou modifiez vos filtres", m_emptyState);
  emptyHint->setStyleSheet(
      "color: #3f3f46; font-size: 12px; background: transparent;");
  emptyHint->setAlignment(Qt::AlignCenter);
  emptyLayout->addWidget(emptyHint);

  m_emptyState->hide();
  contentLayout->addWidget(m_emptyState);

  m_scrollArea->setWidget(scrollContent);
  mainLayout->addWidget(m_scrollArea, 1);
}

// ════════════════════════════════════════════════
//  Context Menu
// ════════════════════════════════════════════════

void MissionsPage::onTableContextMenu(const QPoint &pos) {
  int row = m_table->rowAt(pos.y());
  if (row < 0 || !m_table->item(row, 0))
    return;

  int missionId = m_table->item(row, 0)->data(Qt::UserRole).toInt();
  QString title = m_table->item(row, 0)->text();

  QMenu menu(this);
  menu.setStyleSheet(
      "QMenu { background: #18181b; color: #e4e4e7; border: 1px solid "
      "#27272a; border-radius: 8px; padding: 4px; }"
      "QMenu::item { padding: 8px 20px; border-radius: 4px; }"
      "QMenu::item:selected { background: rgba(245,158,11,0.12); "
      "color: #fcd34d; }"
      "QMenu::separator { height: 1px; background: #27272a; "
      "margin: 4px 8px; }");

  auto *viewAction =
      menu.addAction(QString("%1  Voir la fiche").arg(QChar(0xE716)));
  auto *editAction = menu.addAction(QString("%1  Modifier").arg(QChar(0xE70F)));
  auto *dupAction = menu.addAction(QString("%1  Dupliquer").arg(QChar(0xE8C8)));
  auto *pdfAction =
      menu.addAction(QString("%1  Exporter PDF").arg(QChar(0xE7C3)));

  menu.addSeparator();

  // Quick status submenu
  auto *statusMenu =
      menu.addMenu(QString("%1  Changer le statut").arg(QChar(0xE8AB)));
  statusMenu->setStyleSheet(menu.styleSheet());

  auto *toPlanifiee = statusMenu->addAction("Planifiee");
  auto *toEnCours = statusMenu->addAction("En cours");
  auto *toTerminee = statusMenu->addAction("Terminee");
  auto *toAnnulee = statusMenu->addAction("Annulee");

  // Color dots for status items
  toPlanifiee->setIcon(QIcon());
  toEnCours->setIcon(QIcon());
  toTerminee->setIcon(QIcon());
  toAnnulee->setIcon(QIcon());

  menu.addSeparator();

  auto *deleteAction =
      menu.addAction(QString("%1  Supprimer").arg(QChar(0xE74D)));
  deleteAction->setIcon(QIcon());

  // Execute menu
  QAction *chosen = menu.exec(m_table->viewport()->mapToGlobal(pos));
  if (!chosen)
    return;

  if (chosen == viewAction) {
    onViewMission(row);
  } else if (chosen == editAction) {
    onEditMission(row);
  } else if (chosen == dupAction) {
    onDuplicateMission(row);
  } else if (chosen == pdfAction) {
    onExportPdf(missionId);
  } else if (chosen == deleteAction) {
    if (QMessageBox::question(
            this, "Supprimer",
            QString("Supprimer la mission \"%1\" ?").arg(title)) ==
        QMessageBox::Yes) {
      MissionModel::remove(missionId);
      ActivityLog::log("delete", "mission", "Mission: " + title);
      refresh();
    }
  } else if (chosen == toPlanifiee) {
    onQuickStatusChange(row, "planifiee");
  } else if (chosen == toEnCours) {
    onQuickStatusChange(row, "en_cours");
  } else if (chosen == toTerminee) {
    onQuickStatusChange(row, "terminee");
  } else if (chosen == toAnnulee) {
    onQuickStatusChange(row, "annulee");
  }
}

// ════════════════════════════════════════════════
//  Quick Status Change
// ════════════════════════════════════════════════

void MissionsPage::onQuickStatusChange(int row, const QString &newStatus) {
  if (row < 0 || !m_table->item(row, 0))
    return;
  int missionId = m_table->item(row, 0)->data(Qt::UserRole).toInt();
  Mission m = MissionModel::getById(missionId);
  if (m.id == 0)
    return;

  m.status = newStatus;
  MissionModel::update(m);
  ActivityLog::log("edit", "mission",
                   "Statut -> " + newStatus + ": " + m.title);
  refresh();
}

// ════════════════════════════════════════════════
//  Duplicate Mission
// ════════════════════════════════════════════════

void MissionsPage::onDuplicateMission(int row) {
  if (row < 0 || !m_table->item(row, 0))
    return;
  int missionId = m_table->item(row, 0)->data(Qt::UserRole).toInt();
  Mission original = MissionModel::getById(missionId);
  if (original.id == 0)
    return;

  Mission dup;
  dup.title = original.title + " (copie)";
  dup.clientId = original.clientId;
  dup.address = original.address;
  dup.date = QDate::currentDate().toString("yyyy-MM-dd");
  dup.time = original.time;
  dup.type = original.type;
  dup.notes = original.notes;
  dup.status = "planifiee";

  MissionModel::create(dup);
  ActivityLog::log("create", "mission", "Duplique: " + dup.title);
  refresh();
}

// ════════════════════════════════════════════════
//  Stats
// ════════════════════════════════════════════════

void MissionsPage::updateStats() {
  int planned = MissionModel::countByStatus("planifiee");
  int inProgress = MissionModel::countByStatus("en_cours");
  int done = MissionModel::countByStatus("terminee");
  int cancelled = MissionModel::countByStatus("annulee");

  if (m_statPlanned)
    m_statPlanned->setText(QString::number(planned));
  if (m_statInProgress)
    m_statInProgress->setText(QString::number(inProgress));
  if (m_statDone)
    m_statDone->setText(QString::number(done));
  if (m_statCancelled)
    m_statCancelled->setText(QString::number(cancelled));
}

// ════════════════════════════════════════════════
//  Populate Table
// ════════════════════════════════════════════════

void MissionsPage::populateTable() {
  // Disable sorting while populating to avoid issues
  m_table->setSortingEnabled(false);

  auto allMissions = MissionModel::all();

  // ── Apply filters ──
  QString searchText =
      m_searchInput ? m_searchInput->text().trimmed().toLower() : "";

  QString filterStatus;
  if (m_filterCombo && m_filterCombo->currentIndex() > 0) {
    switch (m_filterCombo->currentIndex()) {
    case 1:
      filterStatus = "planifiee";
      break;
    case 2:
      filterStatus = "en_cours";
      break;
    case 3:
      filterStatus = "terminee";
      break;
    case 4:
      filterStatus = "annulee";
      break;
    }
  }

  QList<Mission> missions;
  for (const auto &m : allMissions) {
    if (m_selectedDate.isValid()) {
      QString dateStr = m_selectedDate.toString("yyyy-MM-dd");
      if (m.date != dateStr)
        continue;
    }
    if (!filterStatus.isEmpty() && m.status != filterStatus)
      continue;
    if (!searchText.isEmpty() && !m.title.toLower().contains(searchText) &&
        !m.clientName.toLower().contains(searchText) &&
        !m.address.toLower().contains(searchText))
      continue;

    missions.append(m);
  }

  // Show/hide empty state
  bool isEmpty = missions.isEmpty();
  m_emptyState->setVisible(isEmpty);
  m_table->setVisible(!isEmpty);

  m_table->setRowCount(missions.size());

  for (int i = 0; i < missions.size(); ++i) {
    const auto &m = missions[i];
    m_table->setRowHeight(i, 48);

    // Title (bold, amber)
    auto *titleItem = new QTableWidgetItem(m.title);
    titleItem->setData(Qt::UserRole, m.id);
    titleItem->setForeground(QColor("#fbbf24"));
    QFont titleFont;
    titleFont.setBold(true);
    titleItem->setFont(titleFont);
    m_table->setItem(i, 0, titleItem);

    // Client
    m_table->setItem(
        i, 1,
        new QTableWidgetItem(m.clientName.isEmpty()
                                 ? QString::fromUtf8("\xe2\x80\x94")
                                 : m.clientName));
    // Date
    m_table->setItem(i, 2, new QTableWidgetItem(m.date));

    // Time
    m_table->setItem(i, 3, new QTableWidgetItem(m.time));

    // Type with color
    auto *typeItem = new QTableWidgetItem(m.type);
    QColor typeColor;
    if (m.type == "photo")
      typeColor = QColor("#f59e0b");
    else if (m.type == "video")
      typeColor = QColor("#10b981");
    else if (m.type == "drone")
      typeColor = QColor("#3b82f6");
    else
      typeColor = QColor("#a855f7");
    typeItem->setForeground(typeColor);
    m_table->setItem(i, 4, typeItem);

    // Status with distinct colors
    auto *statusItem = new QTableWidgetItem(m.status);
    QColor statusColor;
    if (m.status == "planifiee")
      statusColor = QColor("#f59e0b");
    else if (m.status == "en_cours")
      statusColor = QColor("#3b82f6");
    else if (m.status == "terminee")
      statusColor = QColor("#10b981");
    else
      statusColor = QColor("#ef4444");
    statusItem->setForeground(statusColor);
    QFont statusFont;
    statusFont.setBold(true);
    statusFont.setPointSize(9);
    statusItem->setFont(statusFont);
    m_table->setItem(i, 5, statusItem);

    // Photos
    auto *photoItem = new QTableWidgetItem();
    photoItem->setData(Qt::DisplayRole, m.photoCount);
    photoItem->setTextAlignment(Qt::AlignCenter);
    m_table->setItem(i, 6, photoItem);

    // Action buttons
    auto *actionsWidget = new QWidget();
    auto *actionsLayout = new QHBoxLayout(actionsWidget);
    actionsLayout->setContentsMargins(4, 0, 4, 0);
    actionsLayout->setSpacing(4);

    auto *viewBtn = new QPushButton("Fiche");
    viewBtn->setFixedSize(60, 28);
    viewBtn->setCursor(Qt::PointingHandCursor);
    viewBtn->setStyleSheet(
        "QPushButton { background: rgba(245,158,11,0.1); color: #fcd34d; "
        "border: none; border-radius: 6px; font-size: 11px; font-weight: "
        "600; }"
        "QPushButton:hover { background: rgba(245,158,11,0.2); }");
    connect(viewBtn, &QPushButton::clicked, this,
            [this, i]() { onViewMission(i); });
    actionsLayout->addWidget(viewBtn);

    auto *editBtn = new QPushButton("Modifier");
    editBtn->setFixedSize(80, 28);
    editBtn->setCursor(Qt::PointingHandCursor);
    editBtn->setStyleSheet(
        "QPushButton { background: #27272a; color: #a1a1aa; border: none; "
        "border-radius: 6px; font-size: 11px; }"
        "QPushButton:hover { background: #3f3f46; color: white; }");
    connect(editBtn, &QPushButton::clicked, this,
            [this, i]() { onEditMission(i); });
    actionsLayout->addWidget(editBtn);

    auto *delBtn = new QPushButton("\xE2\x9C\x95"); // ✕
    delBtn->setFixedSize(28, 28);
    delBtn->setCursor(Qt::PointingHandCursor);
    delBtn->setStyleSheet(
        "QPushButton { background: #27272a; color: #ef4444; border: none; "
        "border-radius: 6px; font-size: 12px; }"
        "QPushButton:hover { background: #451a1a; }");
    connect(delBtn, &QPushButton::clicked, this, [this, i]() {
      if (!m_table->item(i, 0))
        return;
      int id = m_table->item(i, 0)->data(Qt::UserRole).toInt();
      QString title = m_table->item(i, 0)->text();
      if (QMessageBox::question(
              this, "Supprimer",
              QString("Supprimer la mission \"%1\" ?").arg(title)) ==
          QMessageBox::Yes) {
        MissionModel::remove(id);
        ActivityLog::log("delete", "mission", "Mission: " + title);
        refresh();
      }
    });
    actionsLayout->addWidget(delBtn);

    m_table->setCellWidget(i, 7, actionsWidget);
  }

  // Re-enable sorting
  m_table->setSortingEnabled(true);
}

// ════════════════════════════════════════════════
//  Refresh & Filters
// ════════════════════════════════════════════════

void MissionsPage::refresh() {
  updateStats();
  updateHeaderSubtitle();
  populateTable();
  if (m_calendar) {
    m_calendar->setSelectedDate(m_selectedDate);
    m_calendar->refreshData();
  }
}

void MissionsPage::onSearch(const QString &text) {
  Q_UNUSED(text);
  populateTable();
}

void MissionsPage::onFilterChanged() { populateTable(); }

void MissionsPage::onCalendarDayClicked(QDate date) {
  if (m_selectedDate == date) {
    m_selectedDate = QDate();
    m_dateFilterRow->hide();
  } else {
    m_selectedDate = date;
    m_dateFilterLabel->setText(
        QString("Filtre date : %1").arg(date.toString("dd/MM/yyyy")));
    m_dateFilterRow->show();
  }
  if (m_calendar)
    m_calendar->setSelectedDate(m_selectedDate);
  populateTable();
}

void MissionsPage::onResetDateFilter() {
  m_selectedDate = QDate();
  m_dateFilterRow->hide();
  if (m_calendar)
    m_calendar->setSelectedDate(m_selectedDate);
  populateTable();
}

// ════════════════════════════════════════════════
//  Add Mission
// ════════════════════════════════════════════════

void MissionsPage::onAddMission() {
  QDialog dialog(this);
  dialog.setWindowTitle("Nouvelle mission");
  dialog.setMinimumWidth(520);
  dialog.setStyleSheet(
      "QDialog { background: #09090b; color: #e4e4e7; }"
      "QLabel { color: #a1a1aa; background: transparent; }"
      "QLineEdit, QComboBox, QDateEdit, QTimeEdit, QTextEdit { "
      "  background: #18181b; color: #e4e4e7; border: 1px solid #27272a; "
      "  border-radius: 6px; padding: 6px; }"
      "QLineEdit:focus, QComboBox:focus, QDateEdit:focus, QTimeEdit:focus { "
      "  border-color: #f59e0b; }");

  auto *form = new QFormLayout(&dialog);
  form->setContentsMargins(24, 24, 24, 24);
  form->setSpacing(12);

  auto *titleEdit = new QLineEdit(&dialog);
  titleEdit->setPlaceholderText("Titre de la mission");
  form->addRow("Titre *", titleEdit);

  auto *clientCombo = new QComboBox(&dialog);
  clientCombo->addItem("-- Aucun client --", 0);
  for (const auto &c : ClientModel::all()) {
    clientCombo->addItem(c.name, c.id);
  }
  form->addRow("Client", clientCombo);

  auto *addressEdit = new QLineEdit(&dialog);
  addressEdit->setPlaceholderText("Adresse du shooting");
  form->addRow("Adresse", addressEdit);

  auto *dateEdit = new QDateEdit(QDate::currentDate(), &dialog);
  dateEdit->setCalendarPopup(true);
  dateEdit->setDisplayFormat("dd/MM/yyyy");
  form->addRow("Date", dateEdit);

  auto *timeEdit = new QTimeEdit(QTime(9, 0), &dialog);
  timeEdit->setDisplayFormat("HH:mm");
  form->addRow("Heure", timeEdit);

  auto *typeCombo = new QComboBox(&dialog);
  typeCombo->addItems({"photo", "video", "drone", "visite_virtuelle"});
  form->addRow("Type", typeCombo);

  auto *notesEdit = new QTextEdit(&dialog);
  notesEdit->setPlaceholderText("Notes...");
  notesEdit->setFixedHeight(80);
  form->addRow("Notes", notesEdit);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
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
  buttons->button(QDialogButtonBox::Ok)->setText("Creer");
  buttons->button(QDialogButtonBox::Cancel)->setText("Annuler");
  form->addRow(buttons);

  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  if (dialog.exec() == QDialog::Accepted &&
      !titleEdit->text().trimmed().isEmpty()) {
    Mission m;
    m.title = titleEdit->text().trimmed();
    m.clientId = clientCombo->currentData().toInt();
    m.address = addressEdit->text().trimmed();
    m.date = dateEdit->date().toString("yyyy-MM-dd");
    m.time = timeEdit->time().toString("HH:mm");
    m.type = typeCombo->currentText();
    m.notes = notesEdit->toPlainText().trimmed();

    MissionModel::create(m);
    ActivityLog::log("create", "mission", "Mission: " + m.title);
    refresh();
  }
}

// ════════════════════════════════════════════════
//  Edit Mission
// ════════════════════════════════════════════════

void MissionsPage::onEditMission(int row) {
  if (row < 0 || !m_table->item(row, 0))
    return;
  int missionId = m_table->item(row, 0)->data(Qt::UserRole).toInt();
  Mission m = MissionModel::getById(missionId);
  if (m.id == 0)
    return;

  QDialog dialog(this);
  dialog.setWindowTitle("Modifier mission");
  dialog.setMinimumWidth(520);
  dialog.setStyleSheet(
      "QDialog { background: #09090b; color: #e4e4e7; }"
      "QLabel { color: #a1a1aa; background: transparent; }"
      "QLineEdit, QComboBox, QDateEdit, QTimeEdit, QTextEdit { "
      "  background: #18181b; color: #e4e4e7; border: 1px solid #27272a; "
      "  border-radius: 6px; padding: 6px; }"
      "QLineEdit:focus, QComboBox:focus, QDateEdit:focus, QTimeEdit:focus { "
      "  border-color: #f59e0b; }");

  auto *form = new QFormLayout(&dialog);
  form->setContentsMargins(24, 24, 24, 24);
  form->setSpacing(12);

  auto *titleEdit = new QLineEdit(m.title, &dialog);
  form->addRow("Titre *", titleEdit);

  auto *clientCombo = new QComboBox(&dialog);
  clientCombo->addItem("-- Aucun client --", 0);
  int selectedIndex = 0;
  auto clients = ClientModel::all();
  for (int i = 0; i < clients.size(); ++i) {
    clientCombo->addItem(clients[i].name, clients[i].id);
    if (clients[i].id == m.clientId)
      selectedIndex = i + 1;
  }
  clientCombo->setCurrentIndex(selectedIndex);
  form->addRow("Client", clientCombo);

  auto *addressEdit = new QLineEdit(m.address, &dialog);
  form->addRow("Adresse", addressEdit);

  auto *dateEdit =
      new QDateEdit(QDate::fromString(m.date, "yyyy-MM-dd"), &dialog);
  dateEdit->setCalendarPopup(true);
  dateEdit->setDisplayFormat("dd/MM/yyyy");
  form->addRow("Date", dateEdit);

  auto *timeEdit = new QTimeEdit(QTime::fromString(m.time, "HH:mm"), &dialog);
  timeEdit->setDisplayFormat("HH:mm");
  form->addRow("Heure", timeEdit);

  auto *statusCombo = new QComboBox(&dialog);
  statusCombo->addItems({"planifiee", "en_cours", "terminee", "annulee"});
  statusCombo->setCurrentText(m.status);
  form->addRow("Statut", statusCombo);

  auto *typeCombo = new QComboBox(&dialog);
  typeCombo->addItems({"photo", "video", "drone", "visite_virtuelle"});
  typeCombo->setCurrentText(m.type);
  form->addRow("Type", typeCombo);

  auto *notesEdit = new QTextEdit(&dialog);
  notesEdit->setPlaceholderText("Notes...");
  notesEdit->setFixedHeight(80);
  notesEdit->setPlainText(m.notes);
  form->addRow("Notes", notesEdit);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
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
  buttons->button(QDialogButtonBox::Ok)->setText("Enregistrer");
  buttons->button(QDialogButtonBox::Cancel)->setText("Annuler");

  auto *deleteBtn = new QPushButton("Supprimer", &dialog);
  deleteBtn->setStyleSheet(
      "QPushButton { color: #ef4444; background: rgba(239,68,68,0.1); "
      "border: 1px solid rgba(239,68,68,0.2); border-radius: 6px; "
      "padding: 8px 16px; }"
      "QPushButton:hover { background: rgba(239,68,68,0.2); }");
  buttons->addButton(deleteBtn, QDialogButtonBox::DestructiveRole);
  connect(deleteBtn, &QPushButton::clicked, [&]() { dialog.done(2); });

  auto *dupBtn = new QPushButton("Dupliquer", &dialog);
  dupBtn->setStyleSheet(
      "QPushButton { color: #a1a1aa; background: #27272a; "
      "border: none; border-radius: 6px; padding: 8px 16px; }"
      "QPushButton:hover { background: #3f3f46; color: white; }");
  buttons->addButton(dupBtn, QDialogButtonBox::ActionRole);
  connect(dupBtn, &QPushButton::clicked, [&]() { dialog.done(3); });

  form->addRow(buttons);

  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  int result = dialog.exec();
  if (result == QDialog::Accepted) {
    m.title = titleEdit->text().trimmed();
    m.clientId = clientCombo->currentData().toInt();
    m.address = addressEdit->text().trimmed();
    m.date = dateEdit->date().toString("yyyy-MM-dd");
    m.time = timeEdit->time().toString("HH:mm");
    m.status = statusCombo->currentText();
    m.type = typeCombo->currentText();
    m.notes = notesEdit->toPlainText().trimmed();
    MissionModel::update(m);
    ActivityLog::log("edit", "mission", "Mission: " + m.title);
    refresh();
  } else if (result == 2) {
    auto reply = QMessageBox::warning(
        this, "Supprimer",
        QString("Supprimer la mission \"%1\" ?").arg(m.title),
        QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
      MissionModel::remove(m.id);
      ActivityLog::log("delete", "mission", "Mission: " + m.title);
      refresh();
    }
  } else if (result == 3) {
    // Duplicate
    Mission dup;
    dup.title = m.title + " (copie)";
    dup.clientId = m.clientId;
    dup.address = m.address;
    dup.date = QDate::currentDate().toString("yyyy-MM-dd");
    dup.time = m.time;
    dup.type = m.type;
    dup.notes = m.notes;
    dup.status = "planifiee";
    MissionModel::create(dup);
    ActivityLog::log("create", "mission", "Duplique: " + dup.title);
    refresh();
  }
}

// ════════════════════════════════════════════════
//  View Mission (Fiche)
// ════════════════════════════════════════════════

void MissionsPage::onViewMission(int row) {
  if (row < 0 || !m_table->item(row, 0))
    return;
  int missionId = m_table->item(row, 0)->data(Qt::UserRole).toInt();
  Mission m = MissionModel::getById(missionId);
  if (m.id == 0)
    return;

  QDialog dlg(this);
  dlg.setWindowTitle("Fiche mission");
  dlg.setMinimumSize(680, 540);
  dlg.setStyleSheet("QDialog { background: #09090b; color: #e4e4e7; }"
                    "QLabel { color: #a1a1aa; background: transparent; }");

  auto *mainLayout = new QVBoxLayout(&dlg);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // ── Title bar ──
  auto *titleBar = new QWidget(&dlg);
  titleBar->setStyleSheet(
      "background: #0f0f11; border-bottom: 1px solid #18181b;");
  auto *titleBarLayout = new QHBoxLayout(titleBar);
  titleBarLayout->setContentsMargins(24, 16, 24, 16);

  auto *titleLabel = new QLabel(m.title, titleBar);
  titleLabel->setStyleSheet(
      "color: #fafafa; font-size: 20px; font-weight: 700;");
  titleBarLayout->addWidget(titleLabel);

  titleBarLayout->addStretch();

  // Status badge
  QString statusColor = m.status == "planifiee"  ? "#f59e0b"
                        : m.status == "en_cours" ? "#3b82f6"
                        : m.status == "terminee" ? "#10b981"
                                                 : "#ef4444";
  auto *statusBadge = new QLabel(m.status, titleBar);
  statusBadge->setStyleSheet(
      QString("background: rgba(%1, 0.15); color: %1; border-radius: 6px; "
              "padding: 6px 14px; font-size: 12px; font-weight: 600;")
          .arg(statusColor));
  titleBarLayout->addWidget(statusBadge);

  // Type badge
  QString typeColor = m.type == "photo"   ? "#f59e0b"
                      : m.type == "video" ? "#10b981"
                      : m.type == "drone" ? "#3b82f6"
                                          : "#a855f7";
  auto *typeBadge = new QLabel(m.type, titleBar);
  typeBadge->setStyleSheet(
      QString("background: rgba(%1, 0.15); color: %1; border-radius: 6px; "
              "padding: 6px 14px; font-size: 12px; font-weight: 600;")
          .arg(typeColor));
  titleBarLayout->addWidget(typeBadge);

  mainLayout->addWidget(titleBar);

  // ── Content ──
  auto *contentWidget = new QWidget(&dlg);
  auto *contentLayout = new QVBoxLayout(contentWidget);
  contentLayout->setContentsMargins(24, 20, 24, 20);
  contentLayout->setSpacing(16);

  // Details cards grid
  auto *detailsGrid = new QWidget(contentWidget);
  auto *gridLayout = new QHBoxLayout(detailsGrid);
  gridLayout->setContentsMargins(0, 0, 0, 0);
  gridLayout->setSpacing(16);

  auto addDetailCard = [&](const QString &icon, const QString &label,
                           const QString &value) {
    auto *card = new QWidget(detailsGrid);
    card->setStyleSheet(
        "background: #0f0f11; border: 1px solid rgba(255,255,255,0.04); "
        "border-radius: 10px;");
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(14, 12, 14, 12);
    cardLayout->setSpacing(4);

    auto *iconLbl = new QLabel(icon, card);
    QFont iconFont("Segoe MDL2 Assets", 14);
    iconLbl->setFont(iconFont);
    iconLbl->setStyleSheet("color: #3f3f46; background: transparent;");
    cardLayout->addWidget(iconLbl);

    auto *lblLbl = new QLabel(label, card);
    lblLbl->setStyleSheet(
        "color: #52525b; font-size: 10px; background: transparent; "
        "text-transform: uppercase; letter-spacing: 1px; font-weight: 600;");
    cardLayout->addWidget(lblLbl);

    auto *valLbl = new QLabel(
        value.isEmpty() ? QString::fromUtf8("\xe2\x80\x94") : value, card);
    valLbl->setStyleSheet("color: #e4e4e7; font-size: 14px; font-weight: 500; "
                          "background: transparent;");
    valLbl->setWordWrap(true);
    cardLayout->addWidget(valLbl);

    gridLayout->addWidget(card);
  };

  addDetailCard(QString(QChar(0xE787)), "Date", m.date);
  addDetailCard(QString(QChar(0xE823)), "Heure", m.time);
  addDetailCard(QString(QChar(0xE77B)), "Client", m.clientName);
  addDetailCard(QString(QChar(0xE707)), "Adresse", m.address);

  contentLayout->addWidget(detailsGrid);

  // Notes
  if (!m.notes.isEmpty()) {
    auto *notesCard = new QWidget(contentWidget);
    notesCard->setStyleSheet(
        "background: #0f0f11; border: 1px solid rgba(255,255,255,0.04); "
        "border-radius: 10px;");
    auto *notesLayout = new QVBoxLayout(notesCard);
    notesLayout->setContentsMargins(16, 14, 16, 14);
    notesLayout->setSpacing(6);

    auto *notesTitle = new QLabel("Notes", notesCard);
    notesTitle->setStyleSheet(
        "color: #52525b; font-size: 10px; background: transparent; "
        "text-transform: uppercase; letter-spacing: 1px; font-weight: 600;");
    notesLayout->addWidget(notesTitle);

    auto *notesText = new QLabel(m.notes, notesCard);
    notesText->setStyleSheet(
        "color: #a1a1aa; font-size: 13px; background: transparent;");
    notesText->setWordWrap(true);
    notesLayout->addWidget(notesText);

    contentLayout->addWidget(notesCard);
  }

  // ── Galleries Section ──
  auto galleries = GalleryModel::byMissionId(missionId);
  if (!galleries.isEmpty()) {
    auto *galCard = new QWidget(contentWidget);
    galCard->setStyleSheet(
        "background: #0f0f11; border: 1px solid rgba(255,255,255,0.04); "
        "border-radius: 10px;");
    auto *galLayout = new QVBoxLayout(galCard);
    galLayout->setContentsMargins(16, 14, 16, 14);
    galLayout->setSpacing(8);

    auto *galTitle =
        new QLabel(QString("Galeries (%1)").arg(galleries.size()), galCard);
    galTitle->setStyleSheet(
        "color: #e4e4e7; font-size: 14px; font-weight: 600; "
        "background: transparent;");
    galLayout->addWidget(galTitle);

    for (const auto &g : galleries) {
      auto *galRow = new QWidget(galCard);
      auto *galRowLayout = new QHBoxLayout(galRow);
      galRowLayout->setContentsMargins(0, 4, 0, 4);

      auto *galName = new QLabel(g.title, galRow);
      galName->setStyleSheet(
          "color: #fcd34d; font-weight: 500; font-size: 13px; "
          "background: transparent;");
      galRowLayout->addWidget(galName, 1);

      auto *photoCountLabel =
          new QLabel(QString("%1 photos").arg(g.photos.size()), galRow);
      photoCountLabel->setStyleSheet(
          "color: #52525b; font-size: 12px; background: transparent;");
      galRowLayout->addWidget(photoCountLabel);

      auto *publicBadge = new QLabel(g.isPublic ? "Public" : "Prive", galRow);
      publicBadge->setStyleSheet(
          g.isPublic
              ? "background: rgba(16,185,129,0.15); color: #6ee7b7; "
                "border-radius: 4px; padding: 2px 8px; font-size: 11px;"
              : "background: rgba(113,113,122,0.15); color: #71717a; "
                "border-radius: 4px; padding: 2px 8px; font-size: 11px;");
      galRowLayout->addWidget(publicBadge);
      galLayout->addWidget(galRow);
    }

    contentLayout->addWidget(galCard);
  }

  contentLayout->addStretch();
  mainLayout->addWidget(contentWidget, 1);

  // ── Bottom bar ──
  auto *bottomBar = new QWidget(&dlg);
  bottomBar->setStyleSheet(
      "background: #0f0f11; border-top: 1px solid #18181b;");
  auto *bottomLayout = new QHBoxLayout(bottomBar);
  bottomLayout->setContentsMargins(24, 12, 24, 12);

  bottomLayout->addStretch();

  auto *pdfBtn = new QPushButton("Export PDF", bottomBar);
  pdfBtn->setFixedHeight(36);
  pdfBtn->setCursor(Qt::PointingHandCursor);
  pdfBtn->setStyleSheet(
      "QPushButton { background: rgba(59,130,246,0.1); color: #60a5fa; "
      "border: 1px solid rgba(59,130,246,0.2); border-radius: 8px; "
      "padding: 0 20px; font-weight: 500; }"
      "QPushButton:hover { background: rgba(59,130,246,0.2); }");
  connect(pdfBtn, &QPushButton::clicked, [&]() { onExportPdf(missionId); });
  bottomLayout->addWidget(pdfBtn);

  auto *dupBtn = new QPushButton("Dupliquer", bottomBar);
  dupBtn->setFixedHeight(36);
  dupBtn->setCursor(Qt::PointingHandCursor);
  dupBtn->setStyleSheet(
      "QPushButton { background: #27272a; color: #a1a1aa; border: none; "
      "border-radius: 8px; padding: 0 20px; font-weight: 500; }"
      "QPushButton:hover { background: #3f3f46; color: white; }");
  connect(dupBtn, &QPushButton::clicked, [&]() {
    dlg.accept();
    onDuplicateMission(row);
  });
  bottomLayout->addWidget(dupBtn);

  auto *editBtn = new QPushButton("Modifier", bottomBar);
  editBtn->setFixedHeight(36);
  editBtn->setCursor(Qt::PointingHandCursor);
  editBtn->setStyleSheet(
      "QPushButton { background: #27272a; color: #e4e4e7; border: none; "
      "border-radius: 8px; padding: 0 20px; font-weight: 500; }"
      "QPushButton:hover { background: #3f3f46; }");
  connect(editBtn, &QPushButton::clicked, [&]() {
    dlg.accept();
    onEditMission(row);
  });
  bottomLayout->addWidget(editBtn);

  auto *closeBtn = new QPushButton("Fermer", bottomBar);
  closeBtn->setFixedHeight(36);
  closeBtn->setCursor(Qt::PointingHandCursor);
  closeBtn->setStyleSheet(
      "QPushButton { background: #f59e0b; color: white; border: none; "
      "border-radius: 8px; padding: 0 20px; font-weight: 600; }"
      "QPushButton:hover { background: #fbbf24; }");
  connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
  bottomLayout->addWidget(closeBtn);

  mainLayout->addWidget(bottomBar);

  dlg.exec();
}

// ════════════════════════════════════════════════
//  Export CSV
// ════════════════════════════════════════════════

void MissionsPage::onExportCsv() {
  QString path = QFileDialog::getSaveFileName(this, "Exporter missions",
                                              "missions.csv", "CSV (*.csv)");
  if (path.isEmpty())
    return;

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    return;

  QTextStream out(&file);
  out << "Titre;Client;Date;Heure;Type;Statut;Adresse;Photos\n";

  auto missions = MissionModel::all();
  for (const auto &m : missions) {
    out << m.title << ";" << m.clientName << ";" << m.date << ";" << m.time
        << ";" << m.type << ";" << m.status << ";" << m.address << ";"
        << m.photoCount << "\n";
  }

  file.close();
  ActivityLog::log("export", "mission", "Export CSV missions");
}

// ════════════════════════════════════════════════
//  Batch Delete
// ════════════════════════════════════════════════

void MissionsPage::onBatchDelete() {
  auto selected = m_table->selectionModel()->selectedRows();
  if (selected.isEmpty())
    return;

  int count = selected.size();
  auto reply = QMessageBox::warning(
      this, "Supprimer",
      QString("Supprimer %1 mission(s) selectionnee(s) ?").arg(count),
      QMessageBox::Yes | QMessageBox::No);
  if (reply != QMessageBox::Yes)
    return;

  for (const auto &idx : selected) {
    int id = m_table->item(idx.row(), 0)->data(Qt::UserRole).toInt();
    QString title = m_table->item(idx.row(), 0)->text();
    MissionModel::remove(id);
    ActivityLog::log("delete", "mission", "Mission: " + title);
  }
  refresh();
}

// ════════════════════════════════════════════════
//  PDF Export
// ════════════════════════════════════════════════

void MissionsPage::onExportPdf(int missionId) {
  Mission m = MissionModel::getById(missionId);
  if (m.id == 0)
    return;

  QString defaultName =
      QString("Mission_%1.pdf").arg(m.title.simplified().replace(" ", "_"));
  QString path = QFileDialog::getSaveFileName(
      this, "Exporter fiche mission en PDF", defaultName, "PDF (*.pdf)");
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

  QFont titleFont("Segoe UI", 18, QFont::Bold);
  QFont headerFont("Segoe UI", 11, QFont::Bold);
  QFont normalFont("Segoe UI", 9);
  QFont smallFont("Segoe UI", 8);
  QFont labelFont("Segoe UI", 7);

  // ── Title ──
  painter.setFont(titleFont);
  painter.setPen(QColor("#18181b"));
  painter.drawText(0, y, pageW, 300, Qt::AlignLeft | Qt::AlignTop,
                   "FICHE MISSION");
  y += 350;

  // ── Mission title ──
  painter.setFont(headerFont);
  painter.setPen(QColor("#f59e0b"));
  painter.drawText(0, y, pageW, 250, Qt::AlignLeft, m.title);
  y += 300;

  // ── Status + Type badges ──
  painter.setFont(smallFont);
  painter.setPen(QColor("#3f3f46"));
  painter.drawText(0, y, pageW / 2, 160, Qt::AlignLeft,
                   QString("Statut: %1").arg(m.status));
  painter.drawText(pageW / 2, y, pageW / 2, 160, Qt::AlignLeft,
                   QString("Type: %1").arg(m.type));
  y += 250;

  // ── Separator ──
  painter.setPen(QPen(QColor("#e4e4e7"), 2));
  painter.drawLine(0, y, pageW, y);
  y += 150;

  // ── Details grid ──
  auto drawField = [&](int xPos, int yPos, const QString &label,
                       const QString &value) {
    painter.setFont(labelFont);
    painter.setPen(QColor("#71717a"));
    painter.drawText(xPos, yPos, pageW / 2, 120, Qt::AlignLeft, label);
    painter.setFont(normalFont);
    painter.setPen(QColor("#18181b"));
    painter.drawText(xPos, yPos + 120, pageW / 2, 160, Qt::AlignLeft,
                     value.isEmpty() ? "—" : value);
  };

  drawField(0, y, "DATE", m.date);
  drawField(pageW / 2, y, "HEURE", m.time);
  y += 400;

  drawField(0, y, "CLIENT", m.clientName);
  drawField(pageW / 2, y, "ADRESSE", m.address);
  y += 400;

  drawField(0, y, "NOMBRE DE PHOTOS", QString::number(m.photoCount));
  y += 400;

  // ── Notes ──
  if (!m.notes.isEmpty()) {
    painter.setPen(QPen(QColor("#e4e4e7"), 2));
    painter.drawLine(0, y, pageW, y);
    y += 150;

    painter.setFont(labelFont);
    painter.setPen(QColor("#71717a"));
    painter.drawText(0, y, pageW, 120, Qt::AlignLeft, "NOTES");
    y += 150;

    painter.setFont(normalFont);
    painter.setPen(QColor("#3f3f46"));
    QRect notesRect(0, y, pageW, 1500);
    painter.drawText(notesRect, Qt::AlignLeft | Qt::TextWordWrap, m.notes);
  }

  // ── Footer ──
  painter.setFont(smallFont);
  painter.setPen(QColor("#a1a1aa"));
  int footerY = writer.height() - 200;
  painter.drawText(0, footerY, pageW, 200, Qt::AlignCenter,
                   QString("BlackLys Studio — Genere le %1")
                       .arg(QDate::currentDate().toString("dd/MM/yyyy")));

  painter.end();
  ActivityLog::log("export", "mission", "Export PDF: " + m.title);
}

// ════════════════════════════════════════════════
//  Keyboard Shortcuts
// ════════════════════════════════════════════════

void MissionsPage::setupShortcuts() {
  // Ctrl+N → New mission
  auto *shortcutNew = new QShortcut(QKeySequence("Ctrl+N"), this);
  connect(shortcutNew, &QShortcut::activated, this,
          &MissionsPage::onAddMission);

  // F5 → Refresh
  auto *shortcutRefresh = new QShortcut(QKeySequence("F5"), this);
  connect(shortcutRefresh, &QShortcut::activated, this, &MissionsPage::refresh);

  // Delete → Delete selected
  auto *shortcutDelete = new QShortcut(QKeySequence::Delete, this);
  connect(shortcutDelete, &QShortcut::activated, this, [this]() {
    int row = m_table->currentRow();
    if (row < 0 || !m_table->item(row, 0))
      return;
    int id = m_table->item(row, 0)->data(Qt::UserRole).toInt();
    QString title = m_table->item(row, 0)->text();
    if (QMessageBox::question(
            this, "Supprimer",
            QString("Supprimer la mission \"%1\" ?").arg(title)) ==
        QMessageBox::Yes) {
      MissionModel::remove(id);
      ActivityLog::log("delete", "mission", "Mission: " + title);
      refresh();
    }
  });

  // Escape → Clear all filters
  auto *shortcutEscape = new QShortcut(QKeySequence("Escape"), this);
  connect(shortcutEscape, &QShortcut::activated, this, [this]() {
    m_searchInput->clear();
    m_filterCombo->setCurrentIndex(0);
    onResetDateFilter();
  });

  // Enter → View selected mission
  auto *shortcutEnter = new QShortcut(QKeySequence("Return"), this);
  connect(shortcutEnter, &QShortcut::activated, this, [this]() {
    int row = m_table->currentRow();
    if (row >= 0)
      onViewMission(row);
  });
}

// ════════════════════════════════════════════════
//  Dynamic Header Subtitle
// ════════════════════════════════════════════════

void MissionsPage::updateHeaderSubtitle() {
  if (!m_header)
    return;
  int total = MissionModel::all().size();
  int planned = MissionModel::countByStatus("planifiee");
  int inProgress = MissionModel::countByStatus("en_cours");

  QString sub;
  if (total == 0) {
    sub = "Aucune mission";
  } else {
    QStringList parts;
    parts << QString::number(total) + " mission" + (total > 1 ? "s" : "");
    if (planned > 0)
      parts << QString::number(planned) + " planifiee" +
                   (planned > 1 ? "s" : "");
    if (inProgress > 0)
      parts << QString::number(inProgress) + " en cours";
    sub = parts.join(" · ");
  }
  m_header->setSubtitle(sub);
}
