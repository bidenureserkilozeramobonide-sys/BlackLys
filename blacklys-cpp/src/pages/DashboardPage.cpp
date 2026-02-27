#include "DashboardPage.h"
#include "../database/ActivityLog.h"
#include "../database/ClientModel.h"
#include "../database/InvoiceModel.h"
#include "../database/MissionModel.h"
#include "../database/QuoteModel.h"
#include "../widgets/AnimatedCounter.h"
#include "../widgets/MissionCalendarWidget.h"
#include "../widgets/RevenueChartWidget.h"

#include <QDate>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QTime>
#include <QVBoxLayout>

DashboardPage::DashboardPage(QWidget *parent) : QWidget(parent) {
  setObjectName("page");
  setupUi();
  refresh();

  // Auto-refresh every 2 minutes
  m_refreshTimer = new QTimer(this);
  m_refreshTimer->setInterval(120000);
  connect(m_refreshTimer, &QTimer::timeout, this, &DashboardPage::refresh);
  m_refreshTimer->start();
}

void DashboardPage::setupUi() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // ── Welcome Header ──
  auto *headerWidget = new QWidget(this);
  headerWidget->setObjectName("pageHeader");
  headerWidget->setFixedHeight(80);
  auto *headerLayout = new QVBoxLayout(headerWidget);
  headerLayout->setContentsMargins(32, 16, 32, 16);
  headerLayout->setSpacing(4);

  int hour = QTime::currentTime().hour();
  QString greeting =
      hour < 12 ? "Bonjour" : (hour < 18 ? "Bon apres-midi" : "Bonsoir");

  m_welcomeLabel = new QLabel(greeting, headerWidget);
  m_welcomeLabel->setObjectName("pageTitle");
  QFont welcomeFont("Segoe UI", 20);
  welcomeFont.setWeight(QFont::DemiBold);
  m_welcomeLabel->setFont(welcomeFont);
  headerLayout->addWidget(m_welcomeLabel);

  QLocale frLocale(QLocale::French);
  m_dateLabel =
      new QLabel(frLocale.toString(QDate::currentDate(), "dddd d MMMM yyyy"),
                 headerWidget);
  m_dateLabel->setObjectName("pageSubtitle");
  headerLayout->addWidget(m_dateLabel);
  headerLayout->addStretch();

  layout->addWidget(headerWidget);

  // ── Scrollable Content ──
  auto *scrollArea = new QScrollArea(this);
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setStyleSheet(
      "QScrollArea { background: transparent; border: none; }");

  auto *content = new QWidget(scrollArea);
  content->setObjectName("pageContent");
  auto *contentLayout = new QVBoxLayout(content);
  contentLayout->setContentsMargins(32, 24, 32, 24);
  contentLayout->setSpacing(20);

  // ── Stat Cards Row ── (clickable → navigate to page)
  auto *statsRow = new QWidget(content);
  auto *statsLayout = new QHBoxLayout(statsRow);
  statsLayout->setContentsMargins(0, 0, 0, 0);
  statsLayout->setSpacing(16);

  auto *clientCard =
      createStatCard(QString(QChar(0xE77B)), "0", "Clients", "#f59e0b", 1);
  m_clientCount = clientCard->findChild<AnimatedCounter *>("statValue");
  m_clientTrend = clientCard->findChild<QLabel *>("trendLabel");
  statsLayout->addWidget(clientCard);

  auto *missionCard =
      createStatCard(QString(QChar(0xE7C8)), "0", "Missions", "#f59e0b", 3);
  m_missionCount = missionCard->findChild<AnimatedCounter *>("statValue");
  m_missionTrend = missionCard->findChild<QLabel *>("trendLabel");
  statsLayout->addWidget(missionCard);

  auto *invoiceCard =
      createStatCard(QString(QChar(0xE8C7)), "0", "Factures", "#3b82f6", 6);
  m_invoiceCount = invoiceCard->findChild<AnimatedCounter *>("statValue");
  m_invoiceTrend = invoiceCard->findChild<QLabel *>("trendLabel");
  statsLayout->addWidget(invoiceCard);

  auto *revenueCard = createStatCard(QString(QChar(0xE8A1)), "0 EUR",
                                     "Chiffre d'affaires", "#10b981", 6);
  m_revenue = revenueCard->findChild<AnimatedCounter *>("statValue");
  m_revenueTrend = revenueCard->findChild<QLabel *>("trendLabel");
  statsLayout->addWidget(revenueCard);

  auto *devisCard =
      createStatCard(QString(QChar(0xE873)), "0", "Devis", "#8b5cf6", 2);
  m_devisCount = devisCard->findChild<AnimatedCounter *>("statValue");
  m_devisTrend = devisCard->findChild<QLabel *>("trendLabel");
  statsLayout->addWidget(devisCard);

  contentLayout->addWidget(statsRow);

  // ── Quick Actions ──
  auto *actionsRow = new QWidget(content);
  actionsRow->setObjectName("card");
  auto *actionsLayout = new QHBoxLayout(actionsRow);
  actionsLayout->setContentsMargins(16, 12, 16, 12);
  actionsLayout->setSpacing(12);

  auto *actionsIcon = new QLabel(QString(QChar(0xE945)), actionsRow);
  QFont actIconFont("Segoe MDL2 Assets", 11);
  actionsIcon->setFont(actIconFont);
  actionsIcon->setStyleSheet("color: #52525b; background: transparent;");
  actionsLayout->addWidget(actionsIcon);

  auto *actionsTitle = new QLabel("ACTIONS RAPIDES", actionsRow);
  actionsTitle->setStyleSheet(
      "color: #71717a; font-size: 11px; font-weight: 600; "
      "background: transparent; letter-spacing: 1.5px;");
  actionsLayout->addWidget(actionsTitle);
  actionsLayout->addStretch();

  struct QuickAction {
    QString label;
    QString color;
    int pageIndex;
  };
  QList<QuickAction> actions = {
      {"+ Mission", "#f59e0b", 3},
      {"+ Client", "#f59e0b", 1},
      {"+ Facture", "#3b82f6", 6},
      {"+ Devis", "#8b5cf6", 2},
  };

  for (const auto &a : actions) {
    auto *btn = new QPushButton(a.label, actionsRow);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(34);
    QColor c(a.color);
    btn->setStyleSheet(
        QString("QPushButton { background: rgba(%1,%2,%3,20); color: %4; "
                "border: 1px solid rgba(%1,%2,%3,50); border-radius: 8px; "
                "padding: 0 18px; font-size: 12px; font-weight: 600; }"
                "QPushButton:hover { background: rgba(%1,%2,%3,45); "
                "border-color: rgba(%1,%2,%3,80); }")
            .arg(c.red())
            .arg(c.green())
            .arg(c.blue())
            .arg(a.color));
    int idx = a.pageIndex;
    connect(btn, &QPushButton::clicked, this,
            [this, idx]() { emit quickActionClicked(idx); });
    actionsLayout->addWidget(btn);
  }

  contentLayout->addWidget(actionsRow);

  // ── KPI Strip (conversion rate, overdue, pending, missions done) ──
  auto *kpiRow = new QWidget(content);
  auto *kpiLayout = new QHBoxLayout(kpiRow);
  kpiLayout->setContentsMargins(0, 0, 0, 0);
  kpiLayout->setSpacing(16);

  // Conversion rate devis → factures
  auto *convCard = new QWidget(kpiRow);
  convCard->setObjectName("card");
  auto *convLayout = new QVBoxLayout(convCard);
  convLayout->setContentsMargins(20, 16, 20, 16);
  convLayout->setSpacing(8);

  auto *convHeader = new QHBoxLayout();
  convHeader->setSpacing(8);
  auto *convIcon = new QLabel(QString(QChar(0xE8AB)), convCard);
  QFont convIconFont("Segoe MDL2 Assets", 11);
  convIcon->setFont(convIconFont);
  convIcon->setStyleSheet("color: #10b981; background: transparent;");
  convHeader->addWidget(convIcon);
  auto *convTitle = new QLabel("Conversion devis", convCard);
  convTitle->setStyleSheet(
      "color: #71717a; font-size: 11px; background: transparent;");
  convHeader->addWidget(convTitle);
  convHeader->addStretch();
  convLayout->addLayout(convHeader);

  m_conversionRate = new QLabel("0%", convCard);
  QFont crFont("Segoe UI", 20);
  crFont.setWeight(QFont::Bold);
  m_conversionRate->setFont(crFont);
  m_conversionRate->setStyleSheet("color: #10b981; background: transparent;");
  convLayout->addWidget(m_conversionRate);

  m_conversionBar = new QProgressBar(convCard);
  m_conversionBar->setFixedHeight(6);
  m_conversionBar->setTextVisible(false);
  m_conversionBar->setRange(0, 100);
  m_conversionBar->setValue(0);
  m_conversionBar->setStyleSheet(
      "QProgressBar { background: #18181b; border: none; border-radius: 3px; }"
      "QProgressBar::chunk { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
      "stop:0 #059669, stop:1 #10b981); border-radius: 3px; }");
  convLayout->addWidget(m_conversionBar);

  kpiLayout->addWidget(convCard);

  // Overdue invoices
  auto *overdueCard = new QWidget(kpiRow);
  overdueCard->setObjectName("card");
  overdueCard->setCursor(Qt::PointingHandCursor);
  auto *overdueLayout = new QVBoxLayout(overdueCard);
  overdueLayout->setContentsMargins(20, 16, 20, 16);
  overdueLayout->setSpacing(8);

  auto *overdueHeader = new QHBoxLayout();
  overdueHeader->setSpacing(8);
  auto *overdueIcon = new QLabel(QString(QChar(0xE783)), overdueCard);
  QFont odIconFont("Segoe MDL2 Assets", 11);
  overdueIcon->setFont(odIconFont);
  overdueIcon->setStyleSheet("color: #ef4444; background: transparent;");
  overdueHeader->addWidget(overdueIcon);
  auto *overdueTitle = new QLabel("Factures en retard", overdueCard);
  overdueTitle->setStyleSheet(
      "color: #71717a; font-size: 11px; background: transparent;");
  overdueHeader->addWidget(overdueTitle);
  overdueHeader->addStretch();
  overdueLayout->addLayout(overdueHeader);

  m_overdueCount = new QLabel("0", overdueCard);
  QFont odFont("Segoe UI", 20);
  odFont.setWeight(QFont::Bold);
  m_overdueCount->setFont(odFont);
  m_overdueCount->setStyleSheet("color: #ef4444; background: transparent;");
  overdueLayout->addWidget(m_overdueCount);

  kpiLayout->addWidget(overdueCard);

  // Pending amount
  auto *pendingCard = new QWidget(kpiRow);
  pendingCard->setObjectName("card");
  auto *pendingLayout = new QVBoxLayout(pendingCard);
  pendingLayout->setContentsMargins(20, 16, 20, 16);
  pendingLayout->setSpacing(8);

  auto *pendingHeader = new QHBoxLayout();
  pendingHeader->setSpacing(8);
  auto *pendingIcon = new QLabel(QString(QChar(0xE8C8)), pendingCard);
  QFont pdIconFont("Segoe MDL2 Assets", 11);
  pendingIcon->setFont(pdIconFont);
  pendingIcon->setStyleSheet("color: #f59e0b; background: transparent;");
  pendingHeader->addWidget(pendingIcon);
  auto *pendingTitle = new QLabel("En attente", pendingCard);
  pendingTitle->setStyleSheet(
      "color: #71717a; font-size: 11px; background: transparent;");
  pendingHeader->addWidget(pendingTitle);
  pendingHeader->addStretch();
  pendingLayout->addLayout(pendingHeader);

  m_pendingAmount = new QLabel("0 EUR", pendingCard);
  QFont pdFont("Segoe UI", 20);
  pdFont.setWeight(QFont::Bold);
  m_pendingAmount->setFont(pdFont);
  m_pendingAmount->setStyleSheet("color: #fbbf24; background: transparent;");
  pendingLayout->addWidget(m_pendingAmount);

  kpiLayout->addWidget(pendingCard);

  // Missions completed
  auto *doneCard = new QWidget(kpiRow);
  doneCard->setObjectName("card");
  auto *doneLayout = new QVBoxLayout(doneCard);
  doneLayout->setContentsMargins(20, 16, 20, 16);
  doneLayout->setSpacing(8);

  auto *doneHeader = new QHBoxLayout();
  doneHeader->setSpacing(8);
  auto *doneIcon = new QLabel(QString(QChar(0xE73E)), doneCard);
  QFont dnIconFont("Segoe MDL2 Assets", 11);
  doneIcon->setFont(dnIconFont);
  doneIcon->setStyleSheet("color: #10b981; background: transparent;");
  doneHeader->addWidget(doneIcon);
  auto *doneTitle = new QLabel("Missions terminees", doneCard);
  doneTitle->setStyleSheet(
      "color: #71717a; font-size: 11px; background: transparent;");
  doneHeader->addWidget(doneTitle);
  doneHeader->addStretch();
  doneLayout->addLayout(doneHeader);

  m_missionsDone = new QLabel("0", doneCard);
  QFont dnFont("Segoe UI", 20);
  dnFont.setWeight(QFont::Bold);
  m_missionsDone->setFont(dnFont);
  m_missionsDone->setStyleSheet("color: #6ee7b7; background: transparent;");
  doneLayout->addWidget(m_missionsDone);

  kpiLayout->addWidget(doneCard);

  contentLayout->addWidget(kpiRow);

  // ── Two-Column Layout ──
  auto *columnsRow = new QWidget(content);
  auto *columnsLayout = new QHBoxLayout(columnsRow);
  columnsLayout->setContentsMargins(0, 0, 0, 0);
  columnsLayout->setSpacing(20);

  // ── Left Column (60%) ──
  auto *leftCol = new QWidget(columnsRow);
  auto *leftLayout = new QVBoxLayout(leftCol);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->setSpacing(20);

  // Revenue Chart
  m_chart = new RevenueChartWidget(leftCol);
  leftLayout->addWidget(m_chart);

  // Upcoming Missions
  auto *upcomingSection = new QWidget(leftCol);
  upcomingSection->setObjectName("card");
  auto *upcomingLayout = new QVBoxLayout(upcomingSection);
  upcomingLayout->setContentsMargins(24, 20, 24, 20);
  upcomingLayout->setSpacing(12);

  upcomingLayout->addWidget(
      createSectionHeader(QString(QChar(0xE787)), "Prochaines missions"));

  m_upcomingList = new QWidget(upcomingSection);
  auto *upcomingListLayout = new QVBoxLayout(m_upcomingList);
  upcomingListLayout->setContentsMargins(0, 0, 0, 0);
  upcomingListLayout->setSpacing(8);
  upcomingLayout->addWidget(m_upcomingList);

  leftLayout->addWidget(upcomingSection);
  leftLayout->addStretch();

  columnsLayout->addWidget(leftCol, 3);

  // ── Right Column (40%) ──
  auto *rightCol = new QWidget(columnsRow);
  auto *rightLayout = new QVBoxLayout(rightCol);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  rightLayout->setSpacing(20);

  // Calendar
  m_calendar = new MissionCalendarWidget(rightCol);
  m_calendar->setMinimumHeight(280);
  rightLayout->addWidget(m_calendar);

  // Activity Log
  auto *activitySection = new QWidget(rightCol);
  activitySection->setObjectName("card");
  auto *activityLayout = new QVBoxLayout(activitySection);
  activityLayout->setContentsMargins(24, 20, 24, 20);
  activityLayout->setSpacing(12);

  activityLayout->addWidget(
      createSectionHeader(QString(QChar(0xE81C)), "Activite recente"));

  m_activityList = new QWidget(activitySection);
  auto *actListLayout = new QVBoxLayout(m_activityList);
  actListLayout->setContentsMargins(0, 0, 0, 0);
  actListLayout->setSpacing(6);
  activityLayout->addWidget(m_activityList);

  rightLayout->addWidget(activitySection);

  // Top Clients
  auto *topClientsSection = new QWidget(rightCol);
  topClientsSection->setObjectName("card");
  auto *topClientsLayout = new QVBoxLayout(topClientsSection);
  topClientsLayout->setContentsMargins(24, 20, 24, 20);
  topClientsLayout->setSpacing(12);

  topClientsLayout->addWidget(
      createSectionHeader(QString(QChar(0xE716)), "Top clients (CA)"));

  m_topClientsList = new QWidget(topClientsSection);
  auto *tcListLayout = new QVBoxLayout(m_topClientsList);
  tcListLayout->setContentsMargins(0, 0, 0, 0);
  tcListLayout->setSpacing(6);
  topClientsLayout->addWidget(m_topClientsList);

  rightLayout->addWidget(topClientsSection);
  rightLayout->addStretch();

  columnsLayout->addWidget(rightCol, 2);

  contentLayout->addWidget(columnsRow, 1);

  scrollArea->setWidget(content);
  layout->addWidget(scrollArea, 1);
}

QWidget *DashboardPage::createStatCard(const QString &icon,
                                       const QString &value,
                                       const QString &label,
                                       const QString &color, int targetPage) {
  auto *card = new QWidget(this);
  card->setObjectName("statCard");
  card->setMinimumHeight(100);

  if (targetPage >= 0)
    card->setCursor(Qt::PointingHandCursor);

  QColor accent(color);
  card->setStyleSheet(
      QString("#statCard { background-color: #0f0f11; "
              "border: 1px solid rgba(255,255,255,0.04); "
              "border-radius: 12px; border-left: 3px solid %1; }"
              "#statCard:hover { border-color: rgba(%2,%3,%4,80); "
              "background-color: #111113; }")
          .arg(color)
          .arg(accent.red())
          .arg(accent.green())
          .arg(accent.blue()));

  auto *layout = new QHBoxLayout(card);
  layout->setContentsMargins(20, 16, 20, 16);
  layout->setSpacing(16);

  // Icon with circular tinted background
  auto *iconContainer = new QWidget(card);
  iconContainer->setFixedSize(48, 48);
  iconContainer->setStyleSheet(
      QString("background: rgba(%1,%2,%3,25); border-radius: 24px;")
          .arg(accent.red())
          .arg(accent.green())
          .arg(accent.blue()));

  auto *iconLabel = new QLabel(icon, iconContainer);
  iconLabel->setObjectName("statIcon");
  QFont iconFont("Segoe MDL2 Assets", 18);
  iconLabel->setFont(iconFont);
  iconLabel->setStyleSheet(
      QString("color: %1; background: transparent;").arg(color));
  iconLabel->setFixedSize(48, 48);
  iconLabel->setAlignment(Qt::AlignCenter);
  layout->addWidget(iconContainer);

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
  labelWidget->setObjectName("statLabel");
  labelWidget->setStyleSheet(
      "color: #71717a; font-size: 12px; background: transparent;");
  textLayout->addWidget(labelWidget);

  // Trend indicator
  auto *trendLabel = new QLabel("", card);
  trendLabel->setObjectName("trendLabel");
  trendLabel->setStyleSheet(
      "color: #52525b; font-size: 11px; background: transparent;");
  textLayout->addWidget(trendLabel);

  layout->addLayout(textLayout, 1);

  // Click handler for navigation
  if (targetPage >= 0) {
    auto *overlay = new QPushButton("", card);
    overlay->setCursor(Qt::PointingHandCursor);
    overlay->setStyleSheet(
        "QPushButton { background: transparent; border: none; }");
    overlay->raise();
    connect(overlay, &QPushButton::clicked, this,
            [this, targetPage]() { emit quickActionClicked(targetPage); });
    overlay->setGeometry(0, 0, 9999, 9999);
  }

  return card;
}

QWidget *DashboardPage::createSectionHeader(const QString &icon,
                                            const QString &title) {
  auto *header = new QWidget();
  auto *layout = new QHBoxLayout(header);
  layout->setContentsMargins(0, 0, 0, 4);
  layout->setSpacing(10);

  auto *iconLabel = new QLabel(icon, header);
  QFont iconFont("Segoe MDL2 Assets", 12);
  iconLabel->setFont(iconFont);
  iconLabel->setStyleSheet("color: #f59e0b; background: transparent;");
  layout->addWidget(iconLabel);

  auto *titleLabel = new QLabel(title, header);
  QFont titleFont("Segoe UI", 14);
  titleFont.setWeight(QFont::DemiBold);
  titleLabel->setFont(titleFont);
  titleLabel->setStyleSheet("color: #e4e4e7; background: transparent;");
  layout->addWidget(titleLabel);

  layout->addStretch();
  return header;
}

void DashboardPage::refresh() {
  // ── Helper: format trend label ──
  auto updateTrend = [](QLabel *label, int current, int previous) {
    if (!label)
      return;
    if (previous == 0) {
      label->setText(current > 0 ? QString::fromUtf8("\xe2\x96\xb2 nouveau")
                                 : QString::fromUtf8("\xe2\x80\x94"));
      label->setStyleSheet(
          current > 0
              ? "color: #10b981; font-size: 11px; background: transparent;"
              : "color: #52525b; font-size: 11px; background: transparent;");
      return;
    }
    double pct = ((double)(current - previous) / previous) * 100.0;
    if (pct > 0) {
      label->setText(QString::fromUtf8("\xe2\x96\xb2 +%1%")
                         .arg(QString::number(pct, 'f', 0)));
      label->setStyleSheet(
          "color: #10b981; font-size: 11px; background: transparent;");
    } else if (pct < 0) {
      label->setText(QString::fromUtf8("\xe2\x96\xbc %1%")
                         .arg(QString::number(pct, 'f', 0)));
      label->setStyleSheet(
          "color: #ef4444; font-size: 11px; background: transparent;");
    } else {
      label->setText(QString::fromUtf8("\xe2\x80\x94 0%"));
      label->setStyleSheet(
          "color: #52525b; font-size: 11px; background: transparent;");
    }
  };

  // 30-day-ago date
  QString thirtyDaysAgo =
      QDateTime::currentDateTime().addDays(-30).toString(Qt::ISODate);

  // Update stat cards with animation
  int cc = ClientModel::count();
  int mc = MissionModel::count();
  int ic = InvoiceModel::count();
  double rev = InvoiceModel::totalRevenue();

  if (m_clientCount)
    m_clientCount->setTargetValue(cc);
  if (m_missionCount)
    m_missionCount->setTargetValue(mc);
  if (m_invoiceCount)
    m_invoiceCount->setTargetValue(ic);
  if (m_revenue)
    m_revenue->setTargetCurrency(rev);

  int dc = QuoteModel::count();
  if (m_devisCount)
    m_devisCount->setTargetValue(dc);

  // Update trend indicators
  int ccOld = ClientModel::countBefore(thirtyDaysAgo);
  int mcOld = MissionModel::countBefore(thirtyDaysAgo);
  int icOld = InvoiceModel::countBefore(thirtyDaysAgo);

  updateTrend(m_clientTrend, cc, ccOld);
  updateTrend(m_missionTrend, mc, mcOld);
  updateTrend(m_invoiceTrend, ic, icOld);

  int dcOld = QuoteModel::countBefore(thirtyDaysAgo);
  updateTrend(m_devisTrend, dc, dcOld);

  // Revenue trend
  if (m_revenueTrend) {
    double revOld = InvoiceModel::revenueBefore(thirtyDaysAgo);
    if (revOld < 0.01) {
      m_revenueTrend->setText(rev > 0.01
                                  ? QString::fromUtf8("\xe2\x96\xb2 nouveau")
                                  : QString::fromUtf8("\xe2\x80\x94"));
      m_revenueTrend->setStyleSheet(
          rev > 0.01
              ? "color: #10b981; font-size: 11px; background: transparent;"
              : "color: #52525b; font-size: 11px; background: transparent;");
    } else {
      double pct = ((rev - revOld) / revOld) * 100.0;
      if (pct > 0) {
        m_revenueTrend->setText(QString::fromUtf8("\xe2\x96\xb2 +%1%")
                                    .arg(QString::number(pct, 'f', 0)));
        m_revenueTrend->setStyleSheet(
            "color: #10b981; font-size: 11px; background: transparent;");
      } else if (pct < 0) {
        m_revenueTrend->setText(QString::fromUtf8("\xe2\x96\xbc %1%")
                                    .arg(QString::number(pct, 'f', 0)));
        m_revenueTrend->setStyleSheet(
            "color: #ef4444; font-size: 11px; background: transparent;");
      } else {
        m_revenueTrend->setText(QString::fromUtf8("\xe2\x80\x94 0%"));
        m_revenueTrend->setStyleSheet(
            "color: #52525b; font-size: 11px; background: transparent;");
      }
    }
  }

  // ── Update KPI widgets ──
  // Conversion rate: accepted quotes / total quotes
  if (m_conversionRate && m_conversionBar) {
    int totalDevis = dc; // already computed above
    int accepted = QuoteModel::countByStatus("acceptee");
    if (totalDevis > 0) {
      int rate = (accepted * 100) / totalDevis;
      m_conversionRate->setText(QString("%1%").arg(rate));
      m_conversionBar->setValue(rate);
    } else {
      m_conversionRate->setText(QString::fromUtf8("\xe2\x80\x94"));
      m_conversionBar->setValue(0);
    }
  }

  // Overdue invoices
  if (m_overdueCount) {
    int overdue = InvoiceModel::countOverdue();
    m_overdueCount->setText(QString::number(overdue));
    m_overdueCount->setStyleSheet(
        overdue > 0 ? "color: #ef4444; font-size: 20px; font-weight: 700; "
                      "background: transparent;"
                    : "color: #52525b; font-size: 20px; font-weight: 700; "
                      "background: transparent;");
  }

  // Pending amount
  if (m_pendingAmount) {
    double pending = InvoiceModel::totalPending();
    m_pendingAmount->setText(
        QString("%1 EUR").arg(QString::number(pending, 'f', 0)));
  }

  // Missions completed
  if (m_missionsDone) {
    int done = MissionModel::countByStatus("terminee");
    m_missionsDone->setText(QString::number(done));
  }

  // Refresh chart
  if (m_chart)
    m_chart->refreshData();

  // Refresh calendar
  if (m_calendar)
    m_calendar->refreshData();

  // Update upcoming missions list
  if (m_upcomingList) {
    auto *layout = m_upcomingList->layout();

    QLayoutItem *item;
    while ((item = layout->takeAt(0)) != nullptr) {
      delete item->widget();
      delete item;
    }

    auto missions = MissionModel::upcoming(5);
    if (missions.isEmpty()) {
      auto *emptyW = new QWidget(m_upcomingList);
      auto *emptyL = new QVBoxLayout(emptyW);
      emptyL->setContentsMargins(0, 20, 0, 20);
      emptyL->setAlignment(Qt::AlignCenter);
      emptyL->setSpacing(8);

      auto *emptyIcon = new QLabel(QString(QChar(0xE774)), emptyW);
      QFont ef("Segoe MDL2 Assets", 24);
      emptyIcon->setFont(ef);
      emptyIcon->setStyleSheet("color: #27272a; background: transparent;");
      emptyIcon->setAlignment(Qt::AlignCenter);
      emptyL->addWidget(emptyIcon);

      auto *emptyText = new QLabel("Aucune mission planifiee", emptyW);
      emptyText->setStyleSheet(
          "color: #3f3f46; font-size: 12px; background: transparent;");
      emptyText->setAlignment(Qt::AlignCenter);
      emptyL->addWidget(emptyText);

      layout->addWidget(emptyW);
    } else {
      for (const auto &m : missions) {
        auto *row = new QWidget(m_upcomingList);
        row->setObjectName("missionRow");
        row->setStyleSheet(
            "#missionRow { background: rgba(255,255,255,0.02); "
            "border-radius: 8px; padding: 4px; }"
            "#missionRow:hover { background: rgba(255,255,255,0.04); }");
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(12, 8, 12, 8);

        // Date badge
        auto *dateBadge = new QLabel(m.date, row);
        dateBadge->setStyleSheet(
            "background: rgba(245,158,11,0.15); color: #fcd34d; "
            "border-radius: 4px; padding: 4px 8px; font-size: 11px; "
            "font-weight: 600;");
        dateBadge->setFixedWidth(90);
        dateBadge->setAlignment(Qt::AlignCenter);
        rowLayout->addWidget(dateBadge);

        // Title
        auto *title = new QLabel(m.title, row);
        title->setStyleSheet(
            "color: #e4e4e7; font-weight: 500; background: transparent;");
        rowLayout->addWidget(title, 1);

        // Client
        auto *client = new QLabel(m.clientName, row);
        client->setStyleSheet(
            "color: #71717a; font-size: 12px; background: transparent;");
        rowLayout->addWidget(client);

        // Status badge
        auto *status = new QLabel(m.status, row);
        QColor sc(m.status == "terminee"  ? "#10b981"
                  : m.status == "annulee" ? "#ef4444"
                                          : "#f59e0b");
        status->setStyleSheet(
            QString("background: rgba(%1,%2,%3,38); color: rgb(%1,%2,%3); "
                    "border-radius: 4px; padding: 2px 8px; font-size: 11px;")
                .arg(sc.red())
                .arg(sc.green())
                .arg(sc.blue()));
        rowLayout->addWidget(status);

        layout->addWidget(row);
      }
    }
  }

  // Refresh activity log
  if (m_activityList) {
    auto *layout = m_activityList->layout();
    QLayoutItem *item;
    while ((item = layout->takeAt(0)) != nullptr) {
      delete item->widget();
      delete item;
    }

    auto activities = ActivityLog::recent(8);
    if (activities.isEmpty()) {
      auto *emptyW = new QWidget(m_activityList);
      auto *emptyL = new QVBoxLayout(emptyW);
      emptyL->setContentsMargins(0, 16, 0, 16);
      emptyL->setAlignment(Qt::AlignCenter);
      emptyL->setSpacing(8);

      auto *emptyIcon = new QLabel(QString(QChar(0xE81C)), emptyW);
      QFont ef("Segoe MDL2 Assets", 20);
      emptyIcon->setFont(ef);
      emptyIcon->setStyleSheet("color: #27272a; background: transparent;");
      emptyIcon->setAlignment(Qt::AlignCenter);
      emptyL->addWidget(emptyIcon);

      auto *emptyText = new QLabel("Aucune activite", emptyW);
      emptyText->setStyleSheet(
          "color: #3f3f46; font-size: 12px; background: transparent;");
      emptyText->setAlignment(Qt::AlignCenter);
      emptyL->addWidget(emptyText);

      layout->addWidget(emptyW);
    } else {
      for (const auto &a : activities) {
        auto *row = new QWidget(m_activityList);
        row->setStyleSheet("background: rgba(255,255,255,0.02); "
                           "border-radius: 6px;");
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(12, 6, 12, 6);

        // Action icon
        QString icon = a.action == "create" ? QString(QChar(0xE710))
                       : a.action == "edit" ? QString(QChar(0xE70F))
                                            : QString(QChar(0xE74D));
        QString color = a.action == "create" ? "#10b981"
                        : a.action == "edit" ? "#f59e0b"
                                             : "#ef4444";
        auto *iconLabel = new QLabel(icon, row);
        QFont iconFont("Segoe MDL2 Assets", 11);
        iconLabel->setFont(iconFont);
        iconLabel->setStyleSheet(
            QString("color: %1; background: transparent;").arg(color));
        iconLabel->setFixedWidth(24);
        rowLayout->addWidget(iconLabel);

        // Detail
        auto *detail = new QLabel(a.detail, row);
        detail->setStyleSheet(
            "color: #d4d4d8; font-size: 12px; background: transparent;");
        rowLayout->addWidget(detail, 1);

        // Relative time
        qint64 secs = a.timestamp.secsTo(QDateTime::currentDateTime());
        QString timeStr;
        if (secs < 60)
          timeStr = "a l'instant";
        else if (secs < 3600)
          timeStr = QString("il y a %1 min").arg(secs / 60);
        else if (secs < 86400)
          timeStr = QString("il y a %1h").arg(secs / 3600);
        else
          timeStr = a.timestamp.toString("dd/MM");

        auto *timeLabel = new QLabel(timeStr, row);
        timeLabel->setStyleSheet(
            "color: #52525b; font-size: 11px; background: transparent;");
        rowLayout->addWidget(timeLabel);

        layout->addWidget(row);
      }
    }
  }

  // ── Populate top clients ──
  if (m_topClientsList) {
    auto *layout = m_topClientsList->layout();
    while (layout->count() > 0) {
      auto *item = layout->takeAt(0);
      delete item->widget();
      delete item;
    }

    auto topClients = ClientModel::topByRevenue(5);
    if (topClients.isEmpty()) {
      auto *emptyW = new QWidget(m_topClientsList);
      auto *emptyL = new QVBoxLayout(emptyW);
      emptyL->setContentsMargins(0, 12, 0, 12);
      emptyL->setAlignment(Qt::AlignCenter);
      emptyL->setSpacing(8);

      auto *emptyIcon = new QLabel(QString(QChar(0xE716)), emptyW);
      QFont ef("Segoe MDL2 Assets", 20);
      emptyIcon->setFont(ef);
      emptyIcon->setStyleSheet("color: #27272a; background: transparent;");
      emptyIcon->setAlignment(Qt::AlignCenter);
      emptyL->addWidget(emptyIcon);

      auto *emptyText = new QLabel("Aucun client avec facturation", emptyW);
      emptyText->setStyleSheet(
          "color: #3f3f46; font-size: 12px; background: transparent;");
      emptyText->setAlignment(Qt::AlignCenter);
      emptyL->addWidget(emptyText);

      layout->addWidget(emptyW);
    } else {
      int rank = 1;
      for (const auto &pair : topClients) {
        auto *row = new QWidget(m_topClientsList);
        row->setStyleSheet("background: rgba(255,255,255,0.02); "
                           "border-radius: 6px;");
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(12, 8, 12, 8);
        rowLayout->setSpacing(12);

        // Rank badge
        auto *rankLabel = new QLabel(QString("#%1").arg(rank++), row);
        rankLabel->setStyleSheet(
            "color: #f59e0b; font-size: 11px; font-weight: 700; "
            "background: transparent;");
        rankLabel->setFixedWidth(28);
        rowLayout->addWidget(rankLabel);

        auto *nameLabel = new QLabel(pair.first.name, row);
        nameLabel->setStyleSheet(
            "color: #d4d4d8; font-size: 13px; background: transparent;");
        rowLayout->addWidget(nameLabel, 1);

        auto *revLabel =
            new QLabel(QString::number(pair.second, 'f', 0) + " EUR", row);
        revLabel->setStyleSheet("color: #6ee7b7; font-size: 13px; font-weight: "
                                "600; background: transparent;");
        rowLayout->addWidget(revLabel);

        layout->addWidget(row);
      }
    }
  }
}
