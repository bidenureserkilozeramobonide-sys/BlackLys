#include "SearchOverlay.h"
#include "../database/ClientModel.h"
#include "../database/GalleryModel.h"
#include "../database/InvoiceModel.h"
#include "../database/MissionModel.h"
#include "../database/QuoteModel.h"

#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QKeyEvent>
#include <QPainter>
#include <QPropertyAnimation>

SearchOverlay::SearchOverlay(QWidget *parent) : QWidget(parent) {
  setObjectName("searchOverlay");
  hide();

  // Full-window overlay
  if (parent) {
    setGeometry(parent->rect());
    parent->installEventFilter(this);
  }

  // ── Central panel ──
  m_panel = new QWidget(this);
  m_panel->setFixedWidth(520);
  m_panel->setStyleSheet(
      "QWidget { background: #18181b; border: 1px solid #27272a; "
      "border-radius: 14px; }");

  auto *panelLayout = new QVBoxLayout(m_panel);
  panelLayout->setContentsMargins(16, 16, 16, 12);
  panelLayout->setSpacing(8);

  // Search input
  m_input = new QLineEdit(m_panel);
  m_input->setPlaceholderText("Rechercher clients, missions, factures...");
  m_input->setFixedHeight(44);
  m_input->setStyleSheet("QLineEdit { background: #09090b; color: #e4e4e7; "
                         "border: 1px solid #3f3f46; border-radius: 8px; "
                         "padding: 0 16px; font-size: 14px; }"
                         "QLineEdit:focus { border-color: #f59e0b; }");
  panelLayout->addWidget(m_input);

  // Results list
  m_results = new QListWidget(m_panel);
  m_results->setStyleSheet(
      "QListWidget { background: transparent; border: none; }"
      "QListWidget::item { color: #d4d4d8; padding: 10px 12px; "
      "border-radius: 6px; font-size: 13px; }"
      "QListWidget::item:hover { background: rgba(245,158,11,0.12); }"
      "QListWidget::item:selected { background: rgba(245,158,11,0.2); "
      "color: #fcd34d; }");
  m_results->setMaximumHeight(320);
  m_results->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  panelLayout->addWidget(m_results);

  // Hint
  m_hint = new QLabel("Ctrl+K pour ouvrir  ·  Echap pour fermer", m_panel);
  m_hint->setStyleSheet(
      "color: #3f3f46; font-size: 11px; background: transparent;");
  m_hint->setAlignment(Qt::AlignCenter);
  panelLayout->addWidget(m_hint);

  connect(m_input, &QLineEdit::textChanged, this,
          &SearchOverlay::onTextChanged);
  connect(m_results, &QListWidget::itemClicked, this,
          &SearchOverlay::onItemClicked);
}

void SearchOverlay::toggle() {
  if (m_animating)
    return;

  if (isVisible()) {
    // Animate close
    m_animating = true;
    auto *fadeOut = new QPropertyAnimation(this, "backdropOpacity", this);
    fadeOut->setDuration(150);
    fadeOut->setStartValue(m_backdropAlpha);
    fadeOut->setEndValue(0.0);
    fadeOut->setEasingCurve(QEasingCurve::InQuad);

    // Panel fade out
    auto *panelEffect =
        qobject_cast<QGraphicsOpacityEffect *>(m_panel->graphicsEffect());
    if (panelEffect) {
      auto *panelFade = new QPropertyAnimation(panelEffect, "opacity", this);
      panelFade->setDuration(150);
      panelFade->setStartValue(1.0);
      panelFade->setEndValue(0.0);
      panelFade->setEasingCurve(QEasingCurve::InQuad);
      panelFade->start(QAbstractAnimation::DeleteWhenStopped);
    }

    connect(fadeOut, &QPropertyAnimation::finished, this, [this]() {
      hide();
      m_animating = false;
    });
    fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
  } else {
    if (parentWidget())
      setGeometry(parentWidget()->rect());
    m_input->clear();
    m_results->clear();

    // Reset backdrop to transparent
    m_backdropAlpha = 0;
    show();
    raise();
    m_input->setFocus();

    // Center the panel
    int px = (width() - m_panel->width()) / 2;
    int py = qMax(80, height() / 5);
    m_panel->move(px, py);

    // Ensure panel has an opacity effect
    auto *panelEffect =
        qobject_cast<QGraphicsOpacityEffect *>(m_panel->graphicsEffect());
    if (!panelEffect) {
      panelEffect = new QGraphicsOpacityEffect(m_panel);
      m_panel->setGraphicsEffect(panelEffect);
    }
    panelEffect->setOpacity(0.0);

    // Animate backdrop fade in
    m_animating = true;
    auto *fadeIn = new QPropertyAnimation(this, "backdropOpacity", this);
    fadeIn->setDuration(180);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(140.0);
    fadeIn->setEasingCurve(QEasingCurve::OutQuad);
    connect(fadeIn, &QPropertyAnimation::finished, this,
            [this]() { m_animating = false; });
    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);

    // Animate panel fade in
    auto *panelFade = new QPropertyAnimation(panelEffect, "opacity", this);
    panelFade->setDuration(180);
    panelFade->setStartValue(0.0);
    panelFade->setEndValue(1.0);
    panelFade->setEasingCurve(QEasingCurve::OutQuad);
    panelFade->start(QAbstractAnimation::DeleteWhenStopped);
  }
}

bool SearchOverlay::eventFilter(QObject *obj, QEvent *event) {
  if (obj == parentWidget() && event->type() == QEvent::Resize) {
    setGeometry(parentWidget()->rect());
    int px = (width() - m_panel->width()) / 2;
    int py = qMax(80, height() / 5);
    m_panel->move(px, py);
  }
  return QWidget::eventFilter(obj, event);
}

void SearchOverlay::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.fillRect(rect(), QColor(0, 0, 0, static_cast<int>(m_backdropAlpha)));
}

void SearchOverlay::onTextChanged(const QString &text) {
  m_results->clear();
  if (text.trimmed().length() < 2)
    return;

  // Search clients
  auto clients = ClientModel::search(text);
  for (const auto &c : clients) {
    auto *item = new QListWidgetItem(
        QString("   %1  %2").arg(QChar(0xE77B)).arg(c.name));
    item->setData(Qt::UserRole, 1); // page index
    item->setData(Qt::UserRole + 1, c.id);
    m_results->addItem(item);
  }

  // Search missions
  auto missions = MissionModel::search(text);
  for (const auto &m : missions) {
    auto *item = new QListWidgetItem(
        QString("   %1  %2").arg(QChar(0xE7C8)).arg(m.title));
    item->setData(Qt::UserRole, 3); // page index
    item->setData(Qt::UserRole + 1, m.id);
    m_results->addItem(item);
  }

  // Search invoices
  auto invoices = InvoiceModel::search(text);
  for (const auto &inv : invoices) {
    auto *item = new QListWidgetItem(QString("   %1  %2  —  %3")
                                         .arg(QChar(0xE8C7))
                                         .arg(inv.number)
                                         .arg(inv.clientName));
    item->setData(Qt::UserRole, 6); // page index
    item->setData(Qt::UserRole + 1, inv.id);
    m_results->addItem(item);
  }

  // Search devis
  auto quotes = QuoteModel::search(text);
  for (const auto &qu : quotes) {
    auto *item = new QListWidgetItem(
        QString("   %1  %2").arg(QChar(0xE9F9)).arg(qu.number));
    item->setData(Qt::UserRole, 2); // page index for Devis
    item->setData(Qt::UserRole + 1, qu.id);
    m_results->addItem(item);
  }

  // Search galleries
  auto galleries = GalleryModel::search(text);
  for (const auto &g : galleries) {
    auto *item = new QListWidgetItem(
        QString("   %1  %2").arg(QChar(0xE3B6)).arg(g.title));
    item->setData(Qt::UserRole, 5); // page index for Galleries
    item->setData(Qt::UserRole + 1, g.id);
    m_results->addItem(item);
  }

  if (m_results->count() == 0) {
    auto *empty = new QListWidgetItem("   Aucun resultat");
    empty->setFlags(Qt::NoItemFlags);
    m_results->addItem(empty);
  }
}

void SearchOverlay::onItemClicked(QListWidgetItem *item) {
  int pageIndex = item->data(Qt::UserRole).toInt();
  int itemId = item->data(Qt::UserRole + 1).toInt();
  toggle(); // animated close
  emit resultSelected(pageIndex, itemId);
}

void SearchOverlay::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    toggle(); // animated close
  } else {
    QWidget::keyPressEvent(event);
  }
}

void SearchOverlay::mousePressEvent(QMouseEvent *event) {
  // Click outside the panel → animated dismiss
  if (m_panel && !m_panel->geometry().contains(event->pos())) {
    toggle(); // animated close
  }
  QWidget::mousePressEvent(event);
}
