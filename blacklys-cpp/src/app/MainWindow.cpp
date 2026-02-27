#include "MainWindow.h"
#include "../database/ActivityLog.h"
#include "../database/InvoiceModel.h"
#include "../database/MissionModel.h"
#include "../pages/ClientsPage.h"
#include "../pages/DashboardPage.h"
#include "../pages/DevisPage.h"
#include "../pages/GalleriesPage.h"
#include "../pages/HdrStudioPage.h"
#include "../pages/InvoicesPage.h"
#include "../pages/MissionsPage.h"
#include "../pages/SettingsPage.h"
#include "../widgets/PageHeader.h"
#include "../widgets/SearchOverlay.h"
#include "../widgets/Sidebar.h"
#include "../widgets/StatusBarWidget.h"
#include "../widgets/ToastWidget.h"

#include <QDateTime>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPropertyAnimation>
#include <QShortcut>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
  setWindowTitle("BlackLys — Studio Photo Pro");
  resize(1400, 900);
  setMinimumSize(1024, 680);
  setWindowIcon(QIcon(":/resources/blacklys.ico"));

  setupUi();

  // ── Startup notification: upcoming missions ──
  QTimer::singleShot(1000, this, [this]() {
    auto missions = MissionModel::upcoming(5);
    int soon = 0;
    QString firstName;
    QDateTime threshold = QDateTime::currentDateTime().addSecs(48 * 3600);
    for (const auto &m : missions) {
      QDateTime dt = QDateTime::fromString(m.date, "yyyy-MM-dd");
      if (dt.isValid() && dt <= threshold) {
        ++soon;
        if (firstName.isEmpty())
          firstName = m.title;
      }
    }
    if (soon > 0) {
      QString msg = soon == 1 ? QString("Mission prochaine : %1").arg(firstName)
                              : QString("%1 missions dans les 48h").arg(soon);
      ToastWidget::show(this, msg, ToastWidget::Info);
    }
  });

  // ── Startup notification: overdue invoices ──
  QTimer::singleShot(2000, this, [this]() {
    int overdue = InvoiceModel::countOverdue();
    if (overdue > 0) {
      QString msg = overdue == 1
                        ? "1 facture en retard de paiement"
                        : QString("%1 factures en retard").arg(overdue);
      ToastWidget::show(this, msg, ToastWidget::Error);
    }
  });
}

void MainWindow::setupUi() {
  auto *central = new QWidget(this);
  central->setObjectName("centralWidget");
  auto *layout = new QHBoxLayout(central);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // Sidebar
  m_sidebar = new Sidebar(this);
  m_sidebar->setFixedWidth(180);

  auto *shadow = new QGraphicsDropShadowEffect(this);
  shadow->setBlurRadius(20);
  shadow->setColor(QColor(0, 0, 0, 80));
  shadow->setOffset(2, 0);
  m_sidebar->setGraphicsEffect(shadow);

  layout->addWidget(m_sidebar);

  // Page stack + status bar container
  auto *rightSide = new QWidget(central);
  auto *rightLayout = new QVBoxLayout(rightSide);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  rightLayout->setSpacing(0);

  m_pages = new QStackedWidget(this);
  m_pages->setObjectName("pageStack");
  rightLayout->addWidget(m_pages, 1);

  m_statusBar = new StatusBarWidget(this);
  rightLayout->addWidget(m_statusBar);

  layout->addWidget(rightSide, 1);

  createPages();
  setCentralWidget(central);

  connect(m_sidebar, &Sidebar::pageSelected, this, &MainWindow::navigateTo);

  // Connect dashboard quick actions to navigation
  auto *dashboard = qobject_cast<DashboardPage *>(m_pages->widget(0));
  if (dashboard)
    connect(dashboard, &DashboardPage::quickActionClicked, this,
            &MainWindow::navigateTo);

  // ── Search overlay ──
  m_searchOverlay = new SearchOverlay(central);
  connect(m_searchOverlay, &SearchOverlay::resultSelected, this,
          [this](int pageIndex, int /*itemId*/) { navigateTo(pageIndex); });

  // ── Keyboard shortcuts ──
  auto *searchShortcut = new QShortcut(QKeySequence("Ctrl+K"), this);
  connect(searchShortcut, &QShortcut::activated, m_searchOverlay,
          &SearchOverlay::toggle);

  for (int i = 0; i < 8; ++i) {
    auto *sc = new QShortcut(QKeySequence(QString("Ctrl+%1").arg(i + 1)), this);
    connect(sc, &QShortcut::activated, this, [this, i]() { navigateTo(i); });
  }

  // F11 fullscreen toggle
  auto *fsShortcut = new QShortcut(QKeySequence(Qt::Key_F11), this);
  connect(fsShortcut, &QShortcut::activated, this,
          &MainWindow::toggleFullscreen);

  m_sidebar->setActiveIndex(0);
  m_pages->setCurrentIndex(0);
}

void MainWindow::createPages() {
  m_pages->addWidget(new DashboardPage(this)); // 0 - Dashboard
  m_pages->addWidget(new ClientsPage(this));   // 1 - Clients
  m_pages->addWidget(new DevisPage(this));     // 2 - Devis
  m_pages->addWidget(new MissionsPage(this));  // 3 - Missions
  m_pages->addWidget(new HdrStudioPage(this)); // 4 - HDR Studio
  m_pages->addWidget(new GalleriesPage(this)); // 5 - Galeries
  m_pages->addWidget(new InvoicesPage(this));  // 6 - Facturation
  m_pages->addWidget(new SettingsPage(this));  // 7 - Parametres
}

void MainWindow::navigateTo(int index) {
  if (index < 0 || index >= m_pages->count())
    return;
  if (index == m_pages->currentIndex())
    return;
  if (m_transitioning)
    return;

  m_transitioning = true;

  // Switch the page
  m_pages->setCurrentIndex(index);
  m_sidebar->setActiveIndex(index);

  // Apply a quick fade-in on the new page
  QWidget *incoming = m_pages->currentWidget();
  if (incoming) {
    auto *effect = new QGraphicsOpacityEffect(incoming);
    effect->setOpacity(0.0);
    incoming->setGraphicsEffect(effect);

    auto *fadeIn = new QPropertyAnimation(effect, "opacity", this);
    fadeIn->setDuration(120);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->setEasingCurve(QEasingCurve::OutQuad);
    connect(fadeIn, &QPropertyAnimation::finished, this, [this, incoming]() {
      // Remove the effect once done to avoid permanent GPU overhead
      incoming->setGraphicsEffect(nullptr);
      m_transitioning = false;
    });
    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);
  } else {
    m_transitioning = false;
  }

  // Refresh data for the target page only
  QWidget *page = m_pages->widget(index);
  switch (index) {
  case 0:
    if (auto *p = qobject_cast<DashboardPage *>(page))
      p->refresh();
    break;
  case 1:
    if (auto *p = qobject_cast<ClientsPage *>(page))
      p->refresh();
    break;
  case 2:
    if (auto *p = qobject_cast<DevisPage *>(page))
      p->refresh();
    break;
  case 3:
    if (auto *p = qobject_cast<MissionsPage *>(page))
      p->refresh();
    break;
  case 5:
    if (auto *p = qobject_cast<GalleriesPage *>(page))
      p->refresh();
    break;
  case 6:
    if (auto *p = qobject_cast<InvoicesPage *>(page))
      p->refresh();
    break;
  case 7:
    if (auto *p = qobject_cast<SettingsPage *>(page))
      p->refresh();
    break;
  }

  if (m_statusBar)
    m_statusBar->refresh();

  // Update sidebar badge: show activity count on Dashboard when not viewing
  // it
  if (index == 0) {
    m_sidebar->setBadge(0, 0); // Clear badge when viewing dashboard
  } else {
    int actCount = ActivityLog::recentCount(24); // last 24h
    m_sidebar->setBadge(0, actCount);
  }
}

void MainWindow::toggleFullscreen() {
  m_isFullscreen = !m_isFullscreen;

  if (m_isFullscreen) {
    m_sidebar->hide();
    m_statusBar->hide();
    showFullScreen();
  } else {
    m_sidebar->show();
    m_statusBar->show();
    showNormal();
  }
}
