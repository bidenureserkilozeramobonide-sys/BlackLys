#include "StatusBarWidget.h"
#include "../database/ClientModel.h"
#include "../database/Database.h"
#include "../database/GalleryModel.h"
#include "../database/InvoiceModel.h"
#include "../database/MissionModel.h"
#include "../database/QuoteModel.h"

StatusBarWidget::StatusBarWidget(QWidget *parent) : QWidget(parent) {
  setObjectName("statusBar");
  setFixedHeight(28);
  setStyleSheet(
      "#statusBar { background: #09090b; border-top: 1px solid #18181b; }");

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(16, 0, 16, 0);
  layout->setSpacing(0);

  m_stats = new QLabel(this);
  m_stats->setStyleSheet(
      "color: #52525b; font-size: 11px; background: transparent;");
  layout->addWidget(m_stats);

  layout->addStretch();

  m_dbPath = new QLabel(this);
  m_dbPath->setStyleSheet(
      "color: #3f3f46; font-size: 11px; background: transparent;");
  layout->addWidget(m_dbPath);

  refresh();
}

void StatusBarWidget::refresh() {
  int clients = ClientModel::count();
  int missions = MissionModel::count();
  int invoices = InvoiceModel::count();
  int devis = QuoteModel::count();
  int galleries = GalleryModel::count();

  m_stats->setText(QString("%1 clients  ·  %2 missions  ·  %3 factures  ·  %4 "
                           "devis  ·  %5 galeries")
                       .arg(clients)
                       .arg(missions)
                       .arg(invoices)
                       .arg(devis)
                       .arg(galleries));

  m_dbPath->setText(
      QString("DB: %1").arg(Database::instance().db().databaseName()));
}
