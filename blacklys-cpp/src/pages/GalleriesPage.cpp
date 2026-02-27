#include "GalleriesPage.h"
#include "../database/ActivityLog.h"
#include "../widgets/PageHeader.h"

#include "../widgets/ToastWidget.h"
#include "database/GalleryModel.h"
#include "database/MissionModel.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>
#include <functional>

QString GalleriesPage::relativeTime(const QDateTime &dt) {
  if (!dt.isValid())
    return QString::fromUtf8("\xe2\x80\x94");
  qint64 secs = dt.secsTo(QDateTime::currentDateTime());
  if (secs < 60)
    return "a l'instant";
  if (secs < 3600)
    return QString("il y a %1 min").arg(secs / 60);
  if (secs < 86400)
    return QString("il y a %1h").arg(secs / 3600);
  if (secs < 604800)
    return QString("il y a %1j").arg(secs / 86400);
  return dt.toString("dd/MM/yyyy");
}

GalleriesPage::GalleriesPage(QWidget *parent) : QWidget(parent) {
  setObjectName("page");
  setupUi();
  refresh();
}

// ════════════════════════════════════════════════
//  Stat Card Factory (Dashboard-style)
// ════════════════════════════════════════════════

QWidget *GalleriesPage::createStatCard(const QString &icon,
                                       const QString &value,
                                       const QString &label,
                                       const QString &color) {
  auto *card = new QWidget(this);
  card->setObjectName("statCard");
  card->setMinimumHeight(90);
  card->setStyleSheet(
      QString("#statCard { background-color: #0f0f11; border: 1px solid "
              "rgba(255,255,255,0.04);"
              "border-radius: 12px; border-left: 3px solid %1; }")
          .arg(color));

  auto *cardLayout = new QHBoxLayout(card);
  cardLayout->setContentsMargins(20, 14, 20, 14);
  cardLayout->setSpacing(14);

  // Icon
  auto *iconLabel = new QLabel(icon, card);
  QFont iconFont("Segoe MDL2 Assets", 20);
  iconLabel->setFont(iconFont);
  iconLabel->setStyleSheet(
      QString("color: %1; background: transparent;").arg(color));
  iconLabel->setFixedSize(40, 40);
  iconLabel->setAlignment(Qt::AlignCenter);
  cardLayout->addWidget(iconLabel);

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
      "color: #71717a; font-size: 12px; background: transparent;");
  textLayout->addWidget(labelWidget);

  cardLayout->addLayout(textLayout, 1);

  return card;
}

void GalleriesPage::setupUi() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // Header
  auto *header =
      new PageHeader("Galeries", "Galeries photo pour vos clients", this);
  layout->addWidget(header);

  // Scrollable content
  auto *scrollArea = new QScrollArea(this);
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setStyleSheet("QScrollArea { background: transparent; border: "
                            "none; } QWidget#pageContent { background: "
                            "#09090b; }");

  auto *content = new QWidget(scrollArea);
  content->setObjectName("pageContent");
  auto *contentLayout = new QVBoxLayout(content);
  contentLayout->setContentsMargins(32, 24, 32, 24);
  contentLayout->setSpacing(20);

  // ── Stat Cards Row ──
  auto *statsRow = new QWidget(content);
  auto *statsLayout = new QHBoxLayout(statsRow);
  statsLayout->setContentsMargins(0, 0, 0, 0);
  statsLayout->setSpacing(16);

  auto *totalCard =
      createStatCard(QString(QChar(0xE8B9)), "0", "Total galeries", "#f59e0b");
  m_statTotalValue = totalCard->findChild<QLabel *>("statValue");
  statsLayout->addWidget(totalCard);

  auto *photosCard =
      createStatCard(QString(QChar(0xE722)), "0", "Total photos", "#f59e0b");
  m_statPhotosValue = photosCard->findChild<QLabel *>("statValue");
  statsLayout->addWidget(photosCard);

  auto *publicCard =
      createStatCard(QString(QChar(0xE774)), "0", "Publiques", "#10b981");
  m_statPublicValue = publicCard->findChild<QLabel *>("statValue");
  statsLayout->addWidget(publicCard);

  auto *privateCard =
      createStatCard(QString(QChar(0xE72E)), "0", "Privees", "#6366f1");
  m_statPrivateValue = privateCard->findChild<QLabel *>("statValue");
  statsLayout->addWidget(privateCard);

  contentLayout->addWidget(statsRow);

  // ── Recent Galleries Cards ──
  m_recentCardsContainer = new QWidget(content);
  auto *recentLayout = new QHBoxLayout(m_recentCardsContainer);
  recentLayout->setContentsMargins(0, 0, 0, 0);
  recentLayout->setSpacing(16);
  m_recentCardsContainer->setVisible(false);
  contentLayout->addWidget(m_recentCardsContainer);

  // ── Toolbar ──
  auto *toolbar = new QWidget(content);
  auto *toolbarLayout = new QHBoxLayout(toolbar);
  toolbarLayout->setContentsMargins(0, 0, 0, 0);
  toolbarLayout->setSpacing(12);

  m_searchInput = new QLineEdit(toolbar);
  m_searchInput->setPlaceholderText(
      QString("  %1  Rechercher une galerie...").arg(QChar(0xE721)));
  m_searchInput->setFixedHeight(40);
  m_searchInput->setMinimumWidth(280);
  connect(m_searchInput, &QLineEdit::textChanged, this,
          &GalleriesPage::refresh);
  toolbarLayout->addWidget(m_searchInput);

  // Filter dropdown
  m_filterCombo = new QComboBox(toolbar);
  m_filterCombo->addItem("Toutes", "all");
  m_filterCombo->addItem(QString::fromUtf8("\xe2\x9c\x93 Publiques"), "public");
  m_filterCombo->addItem(QString::fromUtf8("\xf0\x9f\x94\x92 Privees"),
                         "private");
  m_filterCombo->setFixedHeight(40);
  m_filterCombo->setMinimumWidth(140);
  connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this]() { refresh(); });
  toolbarLayout->addWidget(m_filterCombo);

  toolbarLayout->addStretch();

  auto *viewBtn =
      new QPushButton(QString("  %1  Voir").arg(QChar(0xE7B3)), toolbar);
  viewBtn->setFixedHeight(40);
  viewBtn->setCursor(Qt::PointingHandCursor);
  viewBtn->setStyleSheet(
      "QPushButton { background: rgba(245,158,11,0.1); color: #fcd34d; "
      "border: 1px solid rgba(245,158,11,0.25); border-radius: 8px; "
      "padding: 0 16px; font-weight: 600; }"
      "QPushButton:hover { background: rgba(245,158,11,0.2); }");
  connect(viewBtn, &QPushButton::clicked, this, [this]() {
    int row = m_table->currentRow();
    if (row >= 0)
      onViewGallery(row);
  });
  toolbarLayout->addWidget(viewBtn);

  auto *addBtn = new QPushButton(
      QString("  %1  Nouvelle galerie").arg(QChar(0xE710)), toolbar);
  addBtn->setObjectName("primaryButton");
  addBtn->setFixedHeight(40);
  addBtn->setCursor(Qt::PointingHandCursor);
  connect(addBtn, &QPushButton::clicked, this, &GalleriesPage::onAddGallery);
  toolbarLayout->addWidget(addBtn);

  auto *csvBtn = new QPushButton(
      QString("  %1  Exporter CSV").arg(QChar(0xE896)), toolbar);
  csvBtn->setFixedHeight(40);
  csvBtn->setCursor(Qt::PointingHandCursor);
  connect(csvBtn, &QPushButton::clicked, this, &GalleriesPage::onExportCsv);
  toolbarLayout->addWidget(csvBtn);

  auto *batchDelBtn = new QPushButton(
      QString("  %1  Supprimer selection").arg(QChar(0xE74D)), toolbar);
  batchDelBtn->setFixedHeight(40);
  batchDelBtn->setCursor(Qt::PointingHandCursor);
  batchDelBtn->setStyleSheet("color: #ef4444; background: rgba(239,68,68,0.1); "
                             "border: 1px solid rgba(239,68,68,0.2);");
  connect(batchDelBtn, &QPushButton::clicked, this,
          &GalleriesPage::onBatchDelete);
  toolbarLayout->addWidget(batchDelBtn);

  contentLayout->addWidget(toolbar);

  // ── Empty State ──
  m_emptyState = new QWidget(content);
  m_emptyState->setObjectName("card");
  m_emptyState->setMinimumHeight(220);
  auto *emptyLayout = new QVBoxLayout(m_emptyState);
  emptyLayout->setAlignment(Qt::AlignCenter);
  emptyLayout->setSpacing(12);

  auto *emptyIcon = new QLabel(QString(QChar(0xE8B9)), m_emptyState);
  QFont emptyIconFont("Segoe MDL2 Assets", 42);
  emptyIcon->setFont(emptyIconFont);
  emptyIcon->setAlignment(Qt::AlignCenter);
  emptyIcon->setStyleSheet("color: #27272a; background: transparent;");
  emptyLayout->addWidget(emptyIcon);

  auto *emptyTitle = new QLabel("Aucune galerie", m_emptyState);
  emptyTitle->setAlignment(Qt::AlignCenter);
  emptyTitle->setStyleSheet(
      "color: #71717a; font-size: 16px; font-weight: 600; "
      "background: transparent;");
  emptyLayout->addWidget(emptyTitle);

  auto *emptyDesc = new QLabel(
      "Cliquez sur « + Nouvelle galerie » pour creer votre premiere galerie "
      "photo.",
      m_emptyState);
  emptyDesc->setAlignment(Qt::AlignCenter);
  emptyDesc->setWordWrap(true);
  emptyDesc->setStyleSheet(
      "color: #3f3f46; font-size: 13px; background: transparent;");
  emptyLayout->addWidget(emptyDesc);

  m_emptyState->setVisible(false);
  contentLayout->addWidget(m_emptyState);

  // ── Table ──
  m_table = new QTableWidget(content);
  m_table->setColumnCount(7);
  m_table->setHorizontalHeaderLabels(
      {"Titre", "Mission", "Photos", "Statut", "Slug",
       QString::fromUtf8("Cr\xc3\xa9\xc3\xa9 le"), ""});
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setSelectionMode(QAbstractItemView::MultiSelection);
  m_table->verticalHeader()->hide();
  m_table->setAlternatingRowColors(true);
  m_table->setShowGrid(false);
  m_table->setSortingEnabled(true);
  m_table->horizontalHeader()->setStretchLastSection(false);
  m_table->horizontalHeader()->setSortIndicatorShown(true);
  m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_table->horizontalHeader()->setSectionResizeMode(
      1, QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(
      2, QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(
      3, QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
  m_table->horizontalHeader()->setSectionResizeMode(
      5, QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(
      6, QHeaderView::ResizeToContents);
  m_table->setStyleSheet(
      "QTableWidget::item { padding: 8px; }"
      "QTableWidget::item:selected { background: rgba(245,158,11,0.10); }");

  // Context menu
  m_table->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_table, &QTableWidget::customContextMenuRequested, this,
          &GalleriesPage::onContextMenu);

  connect(m_table, &QTableWidget::cellDoubleClicked, this,
          &GalleriesPage::onViewGallery);
  contentLayout->addWidget(m_table, 1);

  scrollArea->setWidget(content);
  layout->addWidget(scrollArea, 1);
}

void GalleriesPage::refresh() {
  m_table->setSortingEnabled(false);
  populateTable();
  m_table->setSortingEnabled(true);
  updateStats();
  populateRecentCards();
}

void GalleriesPage::updateStats() {
  auto allGalleries = GalleryModel::all();
  int total = allGalleries.size();
  int totalPhotos = 0;
  int publicCount = 0;
  int privateCount = 0;
  for (const auto &g : allGalleries) {
    totalPhotos += g.photoCount;
    if (g.isPublic)
      publicCount++;
    else
      privateCount++;
  }
  if (m_statTotalValue)
    m_statTotalValue->setText(QString::number(total));
  if (m_statPhotosValue)
    m_statPhotosValue->setText(QString::number(totalPhotos));
  if (m_statPublicValue)
    m_statPublicValue->setText(QString::number(publicCount));
  if (m_statPrivateValue)
    m_statPrivateValue->setText(QString::number(privateCount));
}

void GalleriesPage::populateTable() {
  auto allGalleries = GalleryModel::all();
  QString search =
      m_searchInput ? m_searchInput->text().trimmed().toLower() : "";
  QString filter =
      m_filterCombo ? m_filterCombo->currentData().toString() : "all";

  QList<Gallery> filtered;
  for (const auto &g : allGalleries) {
    // Text search
    if (!search.isEmpty() && !g.title.toLower().contains(search) &&
        !g.missionTitle.toLower().contains(search) &&
        !g.slug.toLower().contains(search))
      continue;
    // Filter
    if (filter == "public" && !g.isPublic)
      continue;
    if (filter == "private" && g.isPublic)
      continue;
    filtered.append(g);
  }

  // Toggle empty state vs table
  bool isEmpty = filtered.isEmpty();
  m_emptyState->setVisible(isEmpty);
  m_table->setVisible(!isEmpty);

  m_table->setRowCount(filtered.size());
  for (int i = 0; i < filtered.size(); ++i) {
    const auto &g = filtered[i];
    m_table->setRowHeight(i, 48);

    // Title
    auto *titleItem = new QTableWidgetItem(g.title);
    titleItem->setData(Qt::UserRole, g.id);
    titleItem->setForeground(QColor("#fbbf24"));
    QFont titleFont;
    titleFont.setBold(true);
    titleItem->setFont(titleFont);
    m_table->setItem(i, 0, titleItem);

    // Mission
    auto *missionItem = new QTableWidgetItem(
        g.missionTitle.isEmpty() ? QString::fromUtf8("\xe2\x80\x94")
                                 : g.missionTitle);
    missionItem->setForeground(g.missionTitle.isEmpty() ? QColor("#3f3f46")
                                                        : QColor("#a1a1aa"));
    m_table->setItem(i, 1, missionItem);

    // Photo count with icon feel
    auto *countItem = new QTableWidgetItem(
        QString("%1 %2").arg(QChar(0xE722)).arg(g.photoCount));
    countItem->setTextAlignment(Qt::AlignCenter);
    countItem->setForeground(g.photoCount > 0 ? QColor("#d4d4d8")
                                              : QColor("#3f3f46"));
    QFont countFont("Segoe MDL2 Assets", 10);
    m_table->setItem(i, 2, countItem);

    // Status badge (Public / Private)
    auto *statusItem = new QTableWidgetItem(
        g.isPublic ? QString::fromUtf8("\xe2\x9c\x93 Public")
                   : QString::fromUtf8("\xf0\x9f\x94\x92 Prive"));
    statusItem->setForeground(g.isPublic ? QColor("#10b981")
                                         : QColor("#6366f1"));
    QFont statusFont;
    statusFont.setBold(true);
    statusFont.setPointSize(9);
    statusItem->setFont(statusFont);
    m_table->setItem(i, 3, statusItem);

    // Slug
    auto *slugItem = new QTableWidgetItem(g.slug);
    slugItem->setForeground(QColor("#52525b"));
    QFont slugFont;
    slugFont.setPointSize(9);
    slugItem->setFont(slugFont);
    m_table->setItem(i, 4, slugItem);

    // Date (relative time)
    auto *dateItem = new QTableWidgetItem(relativeTime(g.createdAt));
    dateItem->setToolTip(
        g.createdAt.isValid() ? g.createdAt.toString("dd/MM/yyyy HH:mm") : "");
    dateItem->setForeground(QColor("#71717a"));
    QFont dateFont;
    dateFont.setPointSize(10);
    dateItem->setFont(dateFont);
    m_table->setItem(i, 5, dateItem);

    // ── Actions ──
    auto *actionsWidget = new QWidget();
    actionsWidget->setStyleSheet("background: transparent;");
    auto *actionsLayout = new QHBoxLayout(actionsWidget);
    actionsLayout->setContentsMargins(4, 0, 4, 0);
    actionsLayout->setSpacing(4);

    // Toggle public/private
    auto *toggleBtn = new QPushButton(g.isPublic ? QString(QChar(0xE72E))
                                                 : QString(QChar(0xE774)));
    toggleBtn->setFixedSize(28, 28);
    toggleBtn->setCursor(Qt::PointingHandCursor);
    toggleBtn->setToolTip(g.isPublic ? "Rendre privee" : "Rendre publique");
    toggleBtn->setStyleSheet(
        g.isPublic
            ? "QPushButton { background: rgba(99,102,241,0.15); color: #818cf8;"
              " border: none; border-radius: 6px; font-size: 12px; }"
              "QPushButton:hover { background: rgba(99,102,241,0.25); }"
            : "QPushButton { background: rgba(16,185,129,0.15); color: #6ee7b7;"
              " border: none; border-radius: 6px; font-size: 12px; }"
              "QPushButton:hover { background: rgba(16,185,129,0.25); }");
    connect(toggleBtn, &QPushButton::clicked, this,
            [this, id = g.id]() { onTogglePublic(id); });
    actionsLayout->addWidget(toggleBtn);

    // Copy slug
    auto *copyBtn = new QPushButton(QString(QChar(0xE8C8)));
    copyBtn->setFixedSize(28, 28);
    copyBtn->setCursor(Qt::PointingHandCursor);
    copyBtn->setToolTip("Copier le slug");
    copyBtn->setStyleSheet(
        "QPushButton { background: #27272a; color: #a1a1aa; border: none; "
        "border-radius: 6px; font-size: 12px; }"
        "QPushButton:hover { background: #3f3f46; color: white; }");
    connect(copyBtn, &QPushButton::clicked, this,
            [slug = g.slug]() { QApplication::clipboard()->setText(slug); });
    actionsLayout->addWidget(copyBtn);

    // Photos button
    auto *photosBtn =
        new QPushButton(QString("  %1  Photos").arg(QChar(0xE722)));
    photosBtn->setFixedSize(90, 28);
    photosBtn->setCursor(Qt::PointingHandCursor);
    photosBtn->setStyleSheet(
        "QPushButton { background: rgba(245,158,11,0.1); color: #fcd34d; "
        "border: none; border-radius: 6px; font-size: 11px; font-weight: 500; "
        "}"
        "QPushButton:hover { background: rgba(245,158,11,0.2); }");
    connect(photosBtn, &QPushButton::clicked, this,
            [this, id = g.id]() { onManagePhotos(id); });
    actionsLayout->addWidget(photosBtn);

    // Edit
    auto *editBtn = new QPushButton("Modifier");
    editBtn->setFixedSize(80, 28);
    editBtn->setCursor(Qt::PointingHandCursor);
    editBtn->setStyleSheet(
        "QPushButton { background: #27272a; color: #a1a1aa; border: none; "
        "border-radius: 6px; font-size: 11px; }"
        "QPushButton:hover { background: #3f3f46; color: white; }");
    connect(editBtn, &QPushButton::clicked, this,
            [this, i]() { onEditGallery(i); });
    actionsLayout->addWidget(editBtn);

    // Delete
    auto *delBtn = new QPushButton(QString::fromUtf8("\xe2\x9c\x95")); // ✕
    delBtn->setFixedSize(28, 28);
    delBtn->setCursor(Qt::PointingHandCursor);
    delBtn->setStyleSheet(
        "QPushButton { background: #27272a; color: #ef4444; border: none; "
        "border-radius: 6px; font-size: 12px; }"
        "QPushButton:hover { background: #451a1a; }");
    connect(delBtn, &QPushButton::clicked, this, [this, i]() {
      int id = m_table->item(i, 0)->data(Qt::UserRole).toInt();
      if (QMessageBox::question(this, "Supprimer",
                                "Supprimer cette galerie et toutes ses photos "
                                "?") == QMessageBox::Yes) {
        QString title = m_table->item(i, 0)->text();
        GalleryModel::remove(id);
        ActivityLog::log("delete", "galerie", "Galerie: " + title);
        refresh();
      }
    });
    actionsLayout->addWidget(delBtn);

    m_table->setCellWidget(i, 6, actionsWidget);
  }
}

// ════════════════════════════════════════════════
//  Gallery View Dialog
// ════════════════════════════════════════════════

void GalleriesPage::onViewGallery(int row) {
  if (row < 0 || !m_table->item(row, 0))
    return;
  int id = m_table->item(row, 0)->data(Qt::UserRole).toInt();
  Gallery g = GalleryModel::getById(id);
  if (g.id == 0)
    return;

  QDialog dlg(this);
  dlg.setWindowTitle(QString("Galerie — %1").arg(g.title));
  dlg.setMinimumSize(650, 500);
  dlg.setStyleSheet("QDialog { background-color: #0f0f11; }"
                    "QLabel { color: #a1a1aa; background: transparent; }");

  auto *mainLayout = new QVBoxLayout(&dlg);
  mainLayout->setContentsMargins(24, 24, 24, 24);
  mainLayout->setSpacing(16);

  // ── Info Card ──
  auto *card = new QWidget(&dlg);
  card->setStyleSheet(
      "background: rgba(255,255,255,0.02); border-radius: 12px;");
  auto *cardLayout = new QVBoxLayout(card);
  cardLayout->setContentsMargins(20, 16, 20, 16);
  cardLayout->setSpacing(8);

  // Title
  auto *titleLabel = new QLabel(g.title, card);
  titleLabel->setStyleSheet(
      "color: #fafafa; font-size: 20px; font-weight: 700;");
  cardLayout->addWidget(titleLabel);

  // Status + badges row
  auto *metaRow = new QWidget(card);
  auto *metaLayout = new QHBoxLayout(metaRow);
  metaLayout->setContentsMargins(0, 0, 0, 0);
  metaLayout->setSpacing(12);

  auto *statusBadge =
      new QLabel(g.isPublic ? QString::fromUtf8("\xe2\x9c\x93 Publique")
                            : QString::fromUtf8("\xf0\x9f\x94\x92 Privee"),
                 metaRow);
  statusBadge->setStyleSheet(
      g.isPublic ? "background: rgba(16,185,129,0.15); color: #6ee7b7; "
                   "border-radius: 4px; padding: 4px 10px; font-size: 11px; "
                   "font-weight: 600;"
                 : "background: rgba(99,102,241,0.15); color: #818cf8; "
                   "border-radius: 4px; padding: 4px 10px; font-size: 11px; "
                   "font-weight: 600;");
  metaLayout->addWidget(statusBadge);

  auto *photosBadge =
      new QLabel(QString("%1 photo(s)").arg(g.photos.size()), metaRow);
  photosBadge->setStyleSheet(
      "background: rgba(245,158,11,0.15); color: #fcd34d; border-radius: 4px; "
      "padding: 4px 10px; font-size: 11px; font-weight: 600;");
  metaLayout->addWidget(photosBadge);

  metaLayout->addStretch();
  cardLayout->addWidget(metaRow);

  // Details grid
  auto *grid = new QWidget(card);
  auto *gridLayout = new QHBoxLayout(grid);
  gridLayout->setContentsMargins(0, 8, 0, 0);
  gridLayout->setSpacing(24);

  auto addDetail = [&](const QString &label, const QString &value) {
    auto *w = new QWidget(grid);
    auto *vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(2);
    auto *lbl = new QLabel(label, w);
    lbl->setStyleSheet("color: #52525b; font-size: 11px;");
    vl->addWidget(lbl);
    auto *val = new QLabel(
        value.isEmpty() ? QString::fromUtf8("\xe2\x80\x94") : value, w);
    val->setStyleSheet("color: #e4e4e7; font-size: 13px; font-weight: 500;");
    vl->addWidget(val);
    gridLayout->addWidget(w);
  };

  addDetail("Mission", g.missionTitle);
  addDetail("Slug", g.slug);
  addDetail(QString::fromUtf8("Cr\xc3\xa9\xc3\xa9 le"),
            g.createdAt.isValid() ? g.createdAt.toString("dd/MM/yyyy HH:mm")
                                  : "");
  gridLayout->addStretch();
  cardLayout->addWidget(grid);

  mainLayout->addWidget(card);

  // ── Photos Section ──
  if (!g.photos.isEmpty()) {
    auto *photosSection = new QWidget(&dlg);
    photosSection->setStyleSheet(
        "background: rgba(255,255,255,0.02); border-radius: 12px;");
    auto *photosLayout = new QVBoxLayout(photosSection);
    photosLayout->setContentsMargins(20, 16, 20, 16);
    photosLayout->setSpacing(8);

    auto *photosTitle =
        new QLabel(QString("Photos (%1)").arg(g.photos.size()), photosSection);
    photosTitle->setStyleSheet(
        "color: #e4e4e7; font-size: 14px; font-weight: 600;");
    photosLayout->addWidget(photosTitle);

    for (const auto &p : g.photos) {
      auto *photoRow = new QWidget(photosSection);
      auto *photoRowLayout = new QHBoxLayout(photoRow);
      photoRowLayout->setContentsMargins(0, 4, 0, 4);

      // File name (just the basename)
      QString basename = p.filePath;
      int lastSep = basename.lastIndexOf('/');
      if (lastSep < 0)
        lastSep = basename.lastIndexOf('\\');
      if (lastSep >= 0)
        basename = basename.mid(lastSep + 1);

      auto *fileIcon = new QLabel(QString(QChar(0xE8B9)), photoRow);
      QFont fi("Segoe MDL2 Assets", 10);
      fileIcon->setFont(fi);
      fileIcon->setStyleSheet("color: #52525b;");
      fileIcon->setFixedWidth(20);
      photoRowLayout->addWidget(fileIcon);

      auto *fileName = new QLabel(basename, photoRow);
      fileName->setStyleSheet(
          "color: #d4d4d8; font-size: 12px; font-weight: 500;");
      photoRowLayout->addWidget(fileName, 1);

      auto *orderLabel = new QLabel(QString("#%1").arg(p.sortOrder), photoRow);
      orderLabel->setStyleSheet("color: #52525b; font-size: 11px;");
      photoRowLayout->addWidget(orderLabel);

      photosLayout->addWidget(photoRow);
    }

    mainLayout->addWidget(photosSection);
  }

  mainLayout->addStretch();

  // Action buttons row
  auto *actionRow = new QWidget(&dlg);
  auto *actionLayout = new QHBoxLayout(actionRow);
  actionLayout->setContentsMargins(0, 0, 0, 0);
  actionLayout->setSpacing(12);

  auto *editBtn = new QPushButton("Modifier", &dlg);
  editBtn->setFixedHeight(40);
  editBtn->setCursor(Qt::PointingHandCursor);
  editBtn->setStyleSheet(
      "QPushButton { background: #f59e0b; color: white; border: none; "
      "border-radius: 8px; padding: 0 20px; font-weight: 600; }"
      "QPushButton:hover { background: #fbbf24; }");
  connect(editBtn, &QPushButton::clicked, &dlg, [&]() { dlg.done(2); });
  actionLayout->addWidget(editBtn);

  auto *manageBtn = new QPushButton("Gerer les photos", &dlg);
  manageBtn->setFixedHeight(40);
  manageBtn->setCursor(Qt::PointingHandCursor);
  manageBtn->setStyleSheet(
      "QPushButton { background: rgba(245,158,11,0.1); color: #fcd34d; "
      "border: 1px solid rgba(245,158,11,0.25); border-radius: 8px; "
      "padding: 0 20px; font-weight: 600; }"
      "QPushButton:hover { background: rgba(245,158,11,0.2); }");
  connect(manageBtn, &QPushButton::clicked, &dlg, [&]() { dlg.done(3); });
  actionLayout->addWidget(manageBtn);

  auto *copySlugBtn = new QPushButton("Copier le slug", &dlg);
  copySlugBtn->setFixedHeight(40);
  copySlugBtn->setCursor(Qt::PointingHandCursor);
  connect(copySlugBtn, &QPushButton::clicked, this,
          [slug = g.slug]() { QApplication::clipboard()->setText(slug); });
  actionLayout->addWidget(copySlugBtn);

  actionLayout->addStretch();

  auto *closeBtn = new QPushButton("Fermer", &dlg);
  closeBtn->setFixedHeight(40);
  closeBtn->setCursor(Qt::PointingHandCursor);
  connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
  actionLayout->addWidget(closeBtn);

  mainLayout->addWidget(actionRow);

  int result = dlg.exec();
  if (result == 2) {
    onEditGallery(row);
  } else if (result == 3) {
    onManagePhotos(g.id);
  }
}

// ════════════════════════════════════════════════
//  Toggle Public/Private
// ════════════════════════════════════════════════

void GalleriesPage::onTogglePublic(int galleryId) {
  Gallery g = GalleryModel::getById(galleryId);
  if (g.id == 0)
    return;
  g.isPublic = !g.isPublic;
  GalleryModel::update(g);
  ActivityLog::log("edit", "galerie",
                   QString("Galerie %1: %2")
                       .arg(g.title, g.isPublic ? "publique" : "privee"));
  ToastWidget::show(
      this,
      QString("Galerie \"%1\" %2")
          .arg(g.title, g.isPublic ? "rendue publique" : "rendue privee"),
      ToastWidget::Success);
  refresh();
}

// ════════════════════════════════════════════════
//  Gallery Dialog (Create / Edit)
// ════════════════════════════════════════════════

static bool showGalleryDialog(QWidget *parent, Gallery &gallery,
                              bool isNew = true) {
  QDialog dlg(parent);
  dlg.setWindowTitle(isNew ? "Nouvelle galerie" : "Modifier la galerie");
  dlg.setMinimumWidth(480);
  dlg.setStyleSheet(
      "QDialog { background: #09090b; color: #e4e4e7; }"
      "QLabel { color: #a1a1aa; background: transparent; }"
      "QLineEdit, QComboBox { "
      "  background: #18181b; color: #e4e4e7; border: 1px solid #27272a; "
      "  border-radius: 6px; padding: 6px; }"
      "QLineEdit:focus, QComboBox:focus { border-color: #f59e0b; }"
      "QCheckBox { color: #e4e4e7; background: transparent; }"
      "QCheckBox::indicator { width: 18px; height: 18px; }");

  auto *layout = new QVBoxLayout(&dlg);
  layout->setContentsMargins(24, 24, 24, 24);
  layout->setSpacing(16);

  auto *form = new QFormLayout();
  form->setSpacing(10);

  // Title
  auto *titleEdit = new QLineEdit(gallery.title, &dlg);
  titleEdit->setPlaceholderText("Nom de la galerie");
  form->addRow("Titre *", titleEdit);

  // Mission dropdown
  auto *missionCombo = new QComboBox(&dlg);
  missionCombo->addItem(
      QString::fromUtf8("\xe2\x80\x94 Aucune mission \xe2\x80\x94"), 0);
  auto missions = MissionModel::all();
  for (const auto &m : missions) {
    missionCombo->addItem(m.title, m.id);
  }
  for (int i = 0; i < missionCombo->count(); ++i) {
    if (missionCombo->itemData(i).toInt() == gallery.missionId) {
      missionCombo->setCurrentIndex(i);
      break;
    }
  }
  form->addRow("Mission", missionCombo);

  // Public checkbox
  auto *publicCheck = new QCheckBox("Galerie publique", &dlg);
  publicCheck->setChecked(gallery.isPublic);
  form->addRow("Visibilite", publicCheck);

  // Slug (auto-generated)
  auto *slugEdit = new QLineEdit(gallery.slug, &dlg);
  slugEdit->setPlaceholderText("Auto-genere si vide");
  form->addRow("Slug (URL)", slugEdit);

  layout->addLayout(form);

  // Buttons
  auto *buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  buttons->button(QDialogButtonBox::Ok)
      ->setText(isNew ? "Creer" : "Enregistrer");
  buttons->button(QDialogButtonBox::Cancel)->setText("Annuler");
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

  // Delete button (edit mode only)
  if (!isNew) {
    auto *deleteBtn = new QPushButton("Supprimer", &dlg);
    deleteBtn->setStyleSheet("color: #ef4444; background: rgba(239,68,68,0.1); "
                             "border: 1px solid rgba(239,68,68,0.2);");
    buttons->addButton(deleteBtn, QDialogButtonBox::DestructiveRole);
    QObject::connect(deleteBtn, &QPushButton::clicked, [&]() {
      dlg.done(99); // Custom code for delete
    });
  }

  QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg,
                   &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg,
                   &QDialog::reject);
  layout->addWidget(buttons);

  int result = dlg.exec();

  if (result == 99) {
    // Delete requested
    if (QMessageBox::question(parent, "Supprimer",
                              QString("Supprimer la galerie \"%1\" et toutes "
                                      "ses photos ?")
                                  .arg(gallery.title)) == QMessageBox::Yes) {
      GalleryModel::remove(gallery.id);
      ActivityLog::log("delete", "galerie", "Galerie: " + gallery.title);
      gallery.id = -1; // Signal deletion
    }
    return false;
  }

  if (result != QDialog::Accepted)
    return false;

  gallery.title = titleEdit->text().trimmed();
  if (gallery.title.isEmpty()) {
    QMessageBox::warning(parent, "Erreur", "Le titre est requis.");
    return false;
  }
  gallery.missionId = missionCombo->currentData().toInt();
  gallery.isPublic = publicCheck->isChecked();
  gallery.slug = slugEdit->text().trimmed();
  if (gallery.slug.isEmpty()) {
    gallery.slug = GalleryModel::generateSlug(gallery.title);
  }

  return true;
}

void GalleriesPage::onAddGallery() {
  Gallery g;
  if (!showGalleryDialog(this, g, true))
    return;
  GalleryModel::create(g);
  ActivityLog::log("create", "galerie", "Galerie: " + g.title);
  ToastWidget::show(this, QString("Galerie \"%1\" creee").arg(g.title),
                    ToastWidget::Success);
  refresh();
}

void GalleriesPage::onEditGallery(int row) {
  if (row < 0 || !m_table->item(row, 0))
    return;
  int id = m_table->item(row, 0)->data(Qt::UserRole).toInt();
  Gallery g = GalleryModel::getById(id);
  if (g.id == 0)
    return;

  if (!showGalleryDialog(this, g, false)) {
    // Check if deletion was performed
    if (g.id == -1) {
      refresh();
    }
    return;
  }
  GalleryModel::update(g);
  ActivityLog::log("edit", "galerie", "Galerie: " + g.title);
  ToastWidget::show(this, QString("Galerie \"%1\" modifiee").arg(g.title),
                    ToastWidget::Success);
  refresh();
}

void GalleriesPage::onDeleteGallery() {
  int row = m_table->currentRow();
  if (row < 0 || !m_table->item(row, 0))
    return;
  int id = m_table->item(row, 0)->data(Qt::UserRole).toInt();
  if (QMessageBox::question(this, "Supprimer", "Supprimer cette galerie ?") ==
      QMessageBox::Yes) {
    QString title = m_table->item(row, 0) ? m_table->item(row, 0)->text() : "";
    GalleryModel::remove(id);
    ActivityLog::log("delete", "galerie", "Galerie: " + title);
    refresh();
  }
}

void GalleriesPage::onManagePhotos(int galleryId) {
  Gallery g = GalleryModel::getById(galleryId);
  if (g.id == 0)
    return;

  QDialog dlg(this);
  dlg.setWindowTitle(QString("Photos — %1").arg(g.title));
  dlg.setMinimumSize(750, 600);
  dlg.setStyleSheet("QDialog { background: #09090b; color: #e4e4e7; }"
                    "QLabel { color: #a1a1aa; background: transparent; }");

  auto *layout = new QVBoxLayout(&dlg);
  layout->setContentsMargins(24, 24, 24, 24);
  layout->setSpacing(16);

  // Header
  auto *headerRow = new QWidget(&dlg);
  auto *headerLayout = new QHBoxLayout(headerRow);
  headerLayout->setContentsMargins(0, 0, 0, 0);

  auto *dlgTitle = new QLabel(QString("Photos — %1").arg(g.title), headerRow);
  dlgTitle->setStyleSheet("color: #fafafa; font-size: 16px; font-weight: 600;");
  headerLayout->addWidget(dlgTitle);
  headerLayout->addStretch();

  // Cover info
  auto *coverLabel = new QLabel(
      g.coverPath.isEmpty()
          ? "Pas de couverture"
          : QString("Couverture: %1").arg(QFileInfo(g.coverPath).fileName()),
      headerRow);
  coverLabel->setStyleSheet(
      g.coverPath.isEmpty()
          ? "color: #52525b; font-size: 11px;"
          : "background: rgba(16,185,129,0.15); color: #6ee7b7; "
            "border-radius: 4px; padding: 4px 10px; font-size: 11px; "
            "font-weight: 600;");
  headerLayout->addWidget(coverLabel);

  auto *infoLabel =
      new QLabel(QString("%1 photo(s)").arg(g.photos.size()), headerRow);
  infoLabel->setStyleSheet(
      "background: rgba(245,158,11,0.15); color: #fcd34d; border-radius: 4px; "
      "padding: 4px 12px; font-size: 12px; font-weight: 600;");
  headerLayout->addWidget(infoLabel);

  layout->addWidget(headerRow);

  // Photos table — 6 columns: Thumb, Fichier, Couverture, Ordre, Move, Delete
  auto *photoTable = new QTableWidget(&dlg);
  photoTable->setColumnCount(6);
  photoTable->setHorizontalHeaderLabels(
      {"", "Fichier", "Couverture", "Ordre", "", ""});
  photoTable->horizontalHeader()->setSectionResizeMode(
      0, QHeaderView::Fixed); // Thumb
  photoTable->setColumnWidth(0, 56);
  photoTable->horizontalHeader()->setSectionResizeMode(
      1, QHeaderView::Stretch); // Name
  photoTable->horizontalHeader()->setSectionResizeMode(
      2, QHeaderView::ResizeToContents); // Cover
  photoTable->horizontalHeader()->setSectionResizeMode(
      3, QHeaderView::ResizeToContents); // Order
  photoTable->horizontalHeader()->setSectionResizeMode(
      4, QHeaderView::ResizeToContents); // Move
  photoTable->horizontalHeader()->setSectionResizeMode(
      5, QHeaderView::ResizeToContents); // Delete
  photoTable->verticalHeader()->setVisible(false);
  photoTable->setShowGrid(false);
  photoTable->setAlternatingRowColors(true);

  // Lambda to populate the photo table
  std::function<void()> refreshPhotoTable = [&]() {
    auto photos = GalleryModel::getPhotos(galleryId);
    Gallery current = GalleryModel::getById(galleryId);
    photoTable->setRowCount(photos.size());

    for (int i = 0; i < photos.size(); ++i) {
      const auto &p = photos[i];
      photoTable->setRowHeight(i, 56);

      // Thumbnail
      QPixmap pix(p.filePath);
      auto *thumbLabel = new QLabel();
      thumbLabel->setStyleSheet("background: transparent;");
      if (!pix.isNull()) {
        thumbLabel->setPixmap(
            pix.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
      } else {
        QFont iconFont("Segoe MDL2 Assets", 16);
        thumbLabel->setFont(iconFont);
        thumbLabel->setText(QString(QChar(0xE8B9)));
        thumbLabel->setStyleSheet("color: #3f3f46; background: transparent;");
      }
      thumbLabel->setAlignment(Qt::AlignCenter);
      thumbLabel->setFixedSize(52, 52);
      photoTable->setCellWidget(i, 0, thumbLabel);

      // File name (basename only)
      QString basename = QFileInfo(p.filePath).fileName();
      auto *nameItem = new QTableWidgetItem(basename);
      nameItem->setData(Qt::UserRole, p.id);
      nameItem->setToolTip(p.filePath);
      nameItem->setForeground(QColor("#d4d4d8"));
      photoTable->setItem(i, 1, nameItem);

      // Cover star button
      bool isCover = (p.filePath == current.coverPath);
      auto *coverBtn = new QPushButton(isCover ? QString(QChar(0xE735))
                                               : QString(QChar(0xE734)));
      coverBtn->setFixedSize(32, 32);
      coverBtn->setCursor(Qt::PointingHandCursor);
      coverBtn->setToolTip(isCover ? "Couverture actuelle"
                                   : "Definir comme couverture");
      coverBtn->setStyleSheet(
          isCover
              ? "QPushButton { background: rgba(245,158,11,0.2); color: "
                "#fbbf24; border: none; border-radius: 6px; font-size: 14px; }"
                "QPushButton:hover { background: rgba(245,158,11,0.3); }"
              : "QPushButton { background: #27272a; color: #52525b; "
                "border: none; border-radius: 6px; font-size: 14px; }"
                "QPushButton:hover { background: #3f3f46; color: #fbbf24; }");
      QObject::connect(coverBtn, &QPushButton::clicked, [=, &coverLabel]() {
        GalleryModel::setCover(galleryId, p.filePath);
        coverLabel->setText(
            QString("Couverture: %1").arg(QFileInfo(p.filePath).fileName()));
        coverLabel->setStyleSheet(
            "background: rgba(16,185,129,0.15); color: #6ee7b7; "
            "border-radius: 4px; padding: 4px 10px; font-size: 11px; "
            "font-weight: 600;");
        refreshPhotoTable(); // Refresh to update all cover buttons
      });
      auto *coverW = new QWidget();
      coverW->setStyleSheet("background: transparent;");
      auto *coverL = new QHBoxLayout(coverW);
      coverL->setContentsMargins(4, 0, 4, 0);
      coverL->addWidget(coverBtn);
      photoTable->setCellWidget(i, 2, coverW);

      // Order number
      auto *orderItem = new QTableWidgetItem(QString("#%1").arg(p.sortOrder));
      orderItem->setTextAlignment(Qt::AlignCenter);
      orderItem->setForeground(QColor("#52525b"));
      photoTable->setItem(i, 3, orderItem);

      // Move up/down buttons
      auto *moveW = new QWidget();
      moveW->setStyleSheet("background: transparent;");
      auto *moveL = new QHBoxLayout(moveW);
      moveL->setContentsMargins(2, 0, 2, 0);
      moveL->setSpacing(2);

      auto *upBtn = new QPushButton(QString(QChar(0xE70E))); // Up arrow
      upBtn->setFixedSize(24, 24);
      upBtn->setCursor(Qt::PointingHandCursor);
      upBtn->setEnabled(i > 0);
      upBtn->setStyleSheet(
          "QPushButton { background: #27272a; color: #a1a1aa; border: none; "
          "border-radius: 4px; font-size: 10px; }"
          "QPushButton:hover { background: #3f3f46; color: white; }"
          "QPushButton:disabled { color: #27272a; }");
      moveL->addWidget(upBtn);

      auto *downBtn = new QPushButton(QString(QChar(0xE70D))); // Down arrow
      downBtn->setFixedSize(24, 24);
      downBtn->setCursor(Qt::PointingHandCursor);
      downBtn->setEnabled(i < photos.size() - 1);
      downBtn->setStyleSheet(
          "QPushButton { background: #27272a; color: #a1a1aa; border: none; "
          "border-radius: 4px; font-size: 10px; }"
          "QPushButton:hover { background: #3f3f46; color: white; }"
          "QPushButton:disabled { color: #27272a; }");
      moveL->addWidget(downBtn);

      photoTable->setCellWidget(i, 4, moveW);

      // Wire up/down — swap sort orders with neighbor
      if (i > 0) {
        int prevId = photos[i - 1].id;
        int prevOrder = photos[i - 1].sortOrder;
        QObject::connect(upBtn, &QPushButton::clicked,
                         [=, &refreshPhotoTable]() {
                           GalleryModel::updateSortOrder(p.id, prevOrder);
                           GalleryModel::updateSortOrder(prevId, p.sortOrder);
                           refreshPhotoTable();
                         });
      }
      if (i < photos.size() - 1) {
        int nextId = photos[i + 1].id;
        int nextOrder = photos[i + 1].sortOrder;
        QObject::connect(downBtn, &QPushButton::clicked,
                         [=, &refreshPhotoTable]() {
                           GalleryModel::updateSortOrder(p.id, nextOrder);
                           GalleryModel::updateSortOrder(nextId, p.sortOrder);
                           refreshPhotoTable();
                         });
      }

      // Delete button
      auto *removeBtn = new QPushButton(QString::fromUtf8("\xe2\x9c\x95"));
      removeBtn->setFixedSize(28, 28);
      removeBtn->setCursor(Qt::PointingHandCursor);
      removeBtn->setStyleSheet(
          "QPushButton { background: #27272a; color: #ef4444; border: none; "
          "border-radius: 6px; }"
          "QPushButton:hover { background: #451a1a; }");
      QObject::connect(
          removeBtn, &QPushButton::clicked,
          [=, &refreshPhotoTable, &infoLabel]() {
            GalleryModel::removePhoto(p.id);
            infoLabel->setText(
                QString("%1 photo(s)")
                    .arg(GalleryModel::getPhotos(galleryId).size()));
            refreshPhotoTable();
          });
      auto *delW = new QWidget();
      delW->setStyleSheet("background: transparent;");
      auto *delL = new QHBoxLayout(delW);
      delL->setContentsMargins(4, 0, 4, 0);
      delL->addWidget(removeBtn);
      photoTable->setCellWidget(i, 5, delW);
    }

    infoLabel->setText(QString("%1 photo(s)").arg(photos.size()));
  };

  refreshPhotoTable();
  layout->addWidget(photoTable, 1);

  // Bottom buttons
  auto *bottomRow = new QWidget(&dlg);
  auto *bottomLayout = new QHBoxLayout(bottomRow);
  bottomLayout->setContentsMargins(0, 0, 0, 0);
  bottomLayout->setSpacing(12);

  auto *addPhotosBtn = new QPushButton(
      QString("  %1  Ajouter des photos").arg(QChar(0xE710)), &dlg);
  addPhotosBtn->setFixedHeight(40);
  addPhotosBtn->setCursor(Qt::PointingHandCursor);
  addPhotosBtn->setStyleSheet(
      "QPushButton { background: #f59e0b; color: white; border: none; "
      "border-radius: 8px; padding: 0 20px; font-weight: 600; }"
      "QPushButton:hover { background: #fbbf24; }");
  QObject::connect(addPhotosBtn, &QPushButton::clicked,
                   [&dlg, galleryId, &refreshPhotoTable]() {
                     QStringList files = QFileDialog::getOpenFileNames(
                         &dlg, "Ajouter des photos", "",
                         "Images (*.jpg *.jpeg *.png *.tiff *.tif *.bmp "
                         "*.cr2 *.cr3 *.nef *.arw *.dng);;Tous (*)");
                     int order = GalleryModel::getPhotos(galleryId).size();
                     for (const auto &f : files) {
                       GalleryModel::addPhoto(galleryId, f, order++);
                     }
                     if (!files.isEmpty())
                       refreshPhotoTable();
                   });
  bottomLayout->addWidget(addPhotosBtn);
  bottomLayout->addStretch();

  auto *closeBtn = new QPushButton("Fermer", &dlg);
  closeBtn->setFixedHeight(40);
  closeBtn->setCursor(Qt::PointingHandCursor);
  closeBtn->setStyleSheet(
      "QPushButton { background: #27272a; color: #a1a1aa; border: none; "
      "border-radius: 8px; padding: 0 20px; }"
      "QPushButton:hover { background: #3f3f46; color: white; }");
  QObject::connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
  bottomLayout->addWidget(closeBtn);

  layout->addWidget(bottomRow);

  dlg.exec();
  refresh();
}

void GalleriesPage::onBatchDelete() {
  auto selected = m_table->selectionModel()->selectedRows();
  if (selected.isEmpty())
    return;

  int count = selected.size();
  auto reply = QMessageBox::warning(
      this, "Supprimer",
      QString("Supprimer %1 galerie(s) selectionnee(s) ?").arg(count),
      QMessageBox::Yes | QMessageBox::No);
  if (reply != QMessageBox::Yes)
    return;

  for (const auto &idx : selected) {
    int id = m_table->item(idx.row(), 0)->data(Qt::UserRole).toInt();
    QString title = m_table->item(idx.row(), 0)->text();
    GalleryModel::remove(id);
    ActivityLog::log("delete", "galerie", "Galerie: " + title);
  }
  refresh();
}

void GalleriesPage::onExportCsv() {
  QString path = QFileDialog::getSaveFileName(this, "Exporter galeries",
                                              "galeries.csv", "CSV (*.csv)");
  if (path.isEmpty())
    return;

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    return;

  QTextStream out(&file);
  out << "Titre;Mission;Photos;Statut;Slug;Date creation\n";

  auto galleries = GalleryModel::all();
  for (const auto &g : galleries) {
    out << g.title << ";" << g.missionTitle << ";" << g.photoCount << ";"
        << (g.isPublic ? "Public" : "Prive") << ";" << g.slug << ";"
        << (g.createdAt.isValid() ? g.createdAt.toString("dd/MM/yyyy") : "")
        << "\n";
  }

  file.close();
  ActivityLog::log("export", "galerie", "Export CSV galeries");
  ToastWidget::show(this, "Export CSV galeries termine", ToastWidget::Success);
}

// ════════════════════════════════════════════════
//  Recent Gallery Cards
// ════════════════════════════════════════════════

QWidget *GalleriesPage::createRecentCard(int id, const QString &title,
                                         int photoCount, bool isPublic,
                                         const QString &missionTitle) {
  auto *card = new QWidget();
  card->setObjectName("recentCard");
  card->setMinimumHeight(100);
  card->setMaximumHeight(120);
  card->setCursor(Qt::PointingHandCursor);
  card->setStyleSheet(
      "#recentCard { background: #0f0f11; border: 1px solid "
      "rgba(255,255,255,0.04); border-radius: 12px; }"
      "#recentCard:hover { border-color: rgba(245,158,11,0.3); }");

  auto *lay = new QVBoxLayout(card);
  lay->setContentsMargins(16, 12, 16, 12);
  lay->setSpacing(6);

  // Title
  auto *titleLbl = new QLabel(title, card);
  titleLbl->setStyleSheet("color: #fbbf24; font-weight: 600; font-size: 13px; "
                          "background: transparent;");
  titleLbl->setWordWrap(true);
  lay->addWidget(titleLbl);

  // Meta row
  auto *metaRow = new QWidget(card);
  metaRow->setStyleSheet("background: transparent;");
  auto *metaLay = new QHBoxLayout(metaRow);
  metaLay->setContentsMargins(0, 0, 0, 0);
  metaLay->setSpacing(8);

  auto *photoLbl =
      new QLabel(QString("%1 %2").arg(QChar(0xE722)).arg(photoCount), metaRow);
  photoLbl->setStyleSheet(
      "color: #71717a; font-size: 11px; background: transparent;");
  metaLay->addWidget(photoLbl);

  auto *statusDot =
      new QLabel(isPublic ? QString::fromUtf8("\xe2\x97\x8f Public")
                          : QString::fromUtf8("\xe2\x97\x8f Prive"),
                 metaRow);
  statusDot->setStyleSheet(
      isPublic ? "color: #10b981; font-size: 10px; background: transparent;"
               : "color: #6366f1; font-size: 10px; background: transparent;");
  metaLay->addWidget(statusDot);
  metaLay->addStretch();
  lay->addWidget(metaRow);

  // Mission
  if (!missionTitle.isEmpty()) {
    auto *missionLbl = new QLabel(missionTitle, card);
    missionLbl->setStyleSheet(
        "color: #3f3f46; font-size: 10px; background: transparent;");
    lay->addWidget(missionLbl);
  }

  lay->addStretch();

  return card;
}

void GalleriesPage::populateRecentCards() {
  if (!m_recentCardsContainer)
    return;

  // Clear existing cards
  auto *layout = m_recentCardsContainer->layout();
  QLayoutItem *item;
  while ((item = layout->takeAt(0)) != nullptr) {
    delete item->widget();
    delete item;
  }

  auto galleries = GalleryModel::all(); // Already sorted by date DESC
  int count = qMin(4, galleries.size());

  if (count == 0) {
    m_recentCardsContainer->setVisible(false);
    return;
  }

  for (int i = 0; i < count; ++i) {
    const auto &g = galleries[i];
    auto *card = createRecentCard(g.id, g.title, g.photoCount, g.isPublic,
                                  g.missionTitle);
    // Connect click to view
    connect(card, &QWidget::destroyed, this, []() {}); // placeholder
    layout->addWidget(card);
  }
  static_cast<QHBoxLayout *>(layout)->addStretch();

  m_recentCardsContainer->setVisible(true);
}

// ════════════════════════════════════════════════
//  Context Menu
// ════════════════════════════════════════════════

void GalleriesPage::onContextMenu(const QPoint &pos) {
  int row = m_table->rowAt(pos.y());
  if (row < 0 || !m_table->item(row, 0))
    return;

  int id = m_table->item(row, 0)->data(Qt::UserRole).toInt();
  QString title = m_table->item(row, 0)->text();

  QMenu menu(this);
  menu.setStyleSheet(
      "QMenu { background: #18181b; color: #d4d4d8; border: 1px solid "
      "#27272a; border-radius: 8px; padding: 4px; }"
      "QMenu::item { padding: 8px 24px; border-radius: 4px; }"
      "QMenu::item:selected { background: rgba(245,158,11,0.12); }"
      "QMenu::separator { height: 1px; background: #27272a; margin: 4px "
      "8px; }");

  auto *viewAction =
      menu.addAction(QString("%1  Voir la galerie").arg(QChar(0xE7B3)));
  auto *editAction = menu.addAction(QString("%1  Modifier").arg(QChar(0xE70F)));
  auto *photosAction =
      menu.addAction(QString("%1  Gerer les photos").arg(QChar(0xE722)));

  menu.addSeparator();

  Gallery g = GalleryModel::getById(id);
  auto *toggleAction = menu.addAction(
      g.isPublic ? QString("%1  Rendre privee").arg(QChar(0xE72E))
                 : QString("%1  Rendre publique").arg(QChar(0xE774)));
  auto *copyAction =
      menu.addAction(QString("%1  Copier le slug").arg(QChar(0xE8C8)));

  menu.addSeparator();

  auto *deleteAction =
      menu.addAction(QString("%1  Supprimer").arg(QChar(0xE74D)));
  deleteAction->setData("danger");

  QAction *chosen = menu.exec(m_table->viewport()->mapToGlobal(pos));
  if (!chosen)
    return;

  if (chosen == viewAction) {
    onViewGallery(row);
  } else if (chosen == editAction) {
    onEditGallery(row);
  } else if (chosen == photosAction) {
    onManagePhotos(id);
  } else if (chosen == toggleAction) {
    onTogglePublic(id);
  } else if (chosen == copyAction) {
    QApplication::clipboard()->setText(g.slug);
    ToastWidget::show(this, "Slug copie dans le presse-papiers",
                      ToastWidget::Info);
  } else if (chosen == deleteAction) {
    if (QMessageBox::question(
            this, "Supprimer",
            QString("Supprimer la galerie \"%1\" ?").arg(title)) ==
        QMessageBox::Yes) {
      GalleryModel::remove(id);
      ActivityLog::log("delete", "galerie", "Galerie: " + title);
      refresh();
    }
  }
}
