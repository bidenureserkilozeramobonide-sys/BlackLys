#include "Sidebar.h"
#include <QFont>
#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QPainter>
#include <QPropertyAnimation>

// ── Helper: render a Segoe MDL2 Assets glyph into a QIcon (HiDPI) ──
static QIcon mdl2Icon(QChar glyph, const QColor &color, int size = 20) {
  const int scale = 2; // render at 2x for crisp icons
  QPixmap pix(size * scale, size * scale);
  pix.setDevicePixelRatio(scale);
  pix.fill(Qt::transparent);
  QPainter p(&pix);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::TextAntialiasing);
  QFont f("Segoe MDL2 Assets", size - 4);
  f.setHintingPreference(QFont::PreferNoHinting);
  p.setFont(f);
  p.setPen(color);
  p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, QString(glyph));
  p.end();
  return QIcon(pix);
}

Sidebar::Sidebar(QWidget *parent) : QWidget(parent) {
  setObjectName("sidebar");
  setupUi();
}

void Sidebar::setupUi() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // ── Brand header ──
  auto *brandContainer = new QWidget(this);
  brandContainer->setObjectName("brandContainer");
  brandContainer->setFixedHeight(52);
  auto *brandLayout = new QVBoxLayout(brandContainer);
  brandLayout->setContentsMargins(0, 8, 0, 4);

  m_brandLabel = new QLabel("BLACKLYS", this);
  m_brandLabel->setObjectName("brand");
  m_brandLabel->setAlignment(Qt::AlignCenter);
  brandLayout->addWidget(m_brandLabel);

  layout->addWidget(brandContainer);

  // Separator
  auto *sep = new QWidget(this);
  sep->setObjectName("sidebarSeparator");
  sep->setFixedHeight(1);
  layout->addWidget(sep);

  layout->addSpacing(4);

  // ── Navigation items ──
  m_navItems = {
      {QChar(0xE80F), "Dashboard", "Tableau de bord"},
      {QChar(0xE77B), "Clients", "Gestion clients"},
      {QChar(0xE9D5), "Devis", "Devis et estimations"},
      {QChar(0xE7C8), "Missions", "Missions photo"},
      {QChar(0xE722), "HDR Studio", "Editeur HDR"},
      {QChar(0xEB9F), "Galeries", "Galeries publiques"},
      {QChar(0xE8C7), "Facturation", "Facturation"},
  };

  QColor iconColor(161, 161, 170); // #a1a1aa

  for (int i = 0; i < m_navItems.size(); ++i) {
    auto *btn = new QPushButton(this);
    btn->setObjectName("navButton");
    btn->setCheckable(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(40);
    btn->setToolTip(m_navItems[i].tooltip);

    // Icon from Segoe MDL2 Assets font
    btn->setIcon(mdl2Icon(m_navItems[i].icon, iconColor));
    btn->setIconSize(QSize(20, 20));
    btn->setText(QString("  %1").arg(m_navItems[i].label));

    connect(btn, &QPushButton::clicked, this, [this, i]() {
      setActiveIndex(i);
      emit pageSelected(i);
    });

    // Badge label (hidden by default)
    auto *badge = new QLabel(btn);
    badge->setObjectName("navBadge");
    badge->setFixedSize(18, 18);
    badge->setAlignment(Qt::AlignCenter);
    badge->setStyleSheet(
        "background: #ef4444; color: #fff; font-size: 9px; font-weight: 700; "
        "border-radius: 9px; padding: 0;");
    badge->move(btn->width() - 24, 4);
    badge->hide();
    m_badges.append(badge);

    layout->addWidget(btn);
    m_buttons.append(btn);
  }

  // ── Spacer ──
  layout->addStretch(1);

  // ── Bottom section ──
  auto *bottomSep = new QWidget(this);
  bottomSep->setObjectName("sidebarSeparator");
  bottomSep->setFixedHeight(1);
  layout->addWidget(bottomSep);

  // Collapse toggle button
  m_collapseBtn = new QPushButton(this);
  m_collapseBtn->setObjectName("navButtonSecondary");
  m_collapseBtn->setIcon(mdl2Icon(QChar(0xE76B), QColor(113, 113, 122)));
  m_collapseBtn->setIconSize(QSize(18, 18));
  m_collapseBtn->setText("  Reduire");
  m_collapseBtn->setCursor(Qt::PointingHandCursor);
  m_collapseBtn->setFixedHeight(36);
  connect(m_collapseBtn, &QPushButton::clicked, this, &Sidebar::toggleCollapse);
  layout->addWidget(m_collapseBtn);

  // Settings button
  m_settingsBtn = new QPushButton(this);
  m_settingsBtn->setObjectName("navButtonSecondary");
  m_settingsBtn->setIcon(mdl2Icon(QChar(0xE713), QColor(113, 113, 122)));
  m_settingsBtn->setIconSize(QSize(18, 18));
  m_settingsBtn->setText("  Parametres");
  m_settingsBtn->setCursor(Qt::PointingHandCursor);
  m_settingsBtn->setFixedHeight(36);
  connect(m_settingsBtn, &QPushButton::clicked, this,
          [this]() { emit pageSelected(7); });
  layout->addWidget(m_settingsBtn);

  // Version label
  auto *version = new QLabel("v1.0.0", this);
  version->setObjectName("versionLabel");
  version->setAlignment(Qt::AlignCenter);
  version->setFixedHeight(24);
  layout->addWidget(version);
}

void Sidebar::setActiveIndex(int index) {
  if (m_currentIndex == index)
    return;
  m_currentIndex = index;

  for (int i = 0; i < m_buttons.size(); ++i) {
    m_buttons[i]->setChecked(i == index);
  }
}

void Sidebar::setBadge(int index, int count) {
  if (index < 0 || index >= m_badges.size())
    return;
  auto *badge = m_badges[index];
  if (count <= 0) {
    badge->hide();
  } else {
    badge->setText(count > 99 ? "99+" : QString::number(count));
    auto *btn = m_buttons[index];
    badge->move(btn->width() - 26, (btn->height() - 18) / 2);
    badge->show();
    badge->raise();
  }
}

void Sidebar::toggleCollapse() {
  if (m_animating)
    return;

  m_collapsed = !m_collapsed;
  int target = m_collapsed ? m_collapsedWidth : m_expandedWidth;

  // Update texts before animation starts
  updateButtonTexts();

  // Animate width with a single property for clean transitions
  m_animating = true;
  auto *anim = new QPropertyAnimation(this, "sidebarWidth", this);
  anim->setDuration(200);
  anim->setStartValue(width());
  anim->setEndValue(target);
  anim->setEasingCurve(QEasingCurve::OutQuad);
  connect(anim, &QPropertyAnimation::finished, this,
          [this]() { m_animating = false; });
  anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void Sidebar::updateButtonTexts() {
  QColor iconColor(161, 161, 170);
  QColor secColor(113, 113, 122);

  if (m_collapsed) {
    m_brandLabel->setText("B");
    for (int i = 0; i < m_buttons.size(); ++i) {
      m_buttons[i]->setText("");
    }
    m_collapseBtn->setText("");
    m_collapseBtn->setIcon(mdl2Icon(QChar(0xE76C), secColor)); // expand arrow
    m_settingsBtn->setText("");
  } else {
    m_brandLabel->setText("BLACKLYS");
    for (int i = 0; i < m_buttons.size(); ++i) {
      m_buttons[i]->setText(QString("  %1").arg(m_navItems[i].label));
    }
    m_collapseBtn->setText("  Reduire");
    m_collapseBtn->setIcon(mdl2Icon(QChar(0xE76B), secColor));
    m_settingsBtn->setText("  Parametres");
  }
}
