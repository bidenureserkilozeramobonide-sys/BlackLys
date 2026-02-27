#include "RevenueChartWidget.h"
#include "../database/Database.h"
#include "../database/InvoiceModel.h"

#include <QDate>
#include <QPaintEvent>
#include <QPainter>
#include <QSqlQuery>

RevenueChartWidget::RevenueChartWidget(QWidget *parent) : QWidget(parent) {
  setMinimumHeight(220);
  setObjectName("revenueChart");
  refreshData();
}

void RevenueChartWidget::refreshData() {
  m_months.clear();
  m_maxRevenue = 0.0;

  QDate today = QDate::currentDate();

  for (int i = 5; i >= 0; --i) {
    QDate month = today.addMonths(-i);
    QString yearMonth = month.toString("yyyy-MM");

    QSqlQuery q(Database::instance().db());
    q.prepare(R"(
            SELECT COALESCE(SUM(total), 0)
            FROM invoices
            WHERE status = 'payee'
              AND strftime('%Y-%m', date) = ?
        )");
    q.addBindValue(yearMonth);
    q.exec();

    MonthData md;
    md.label = month.toString("MMM");
    md.revenue = q.next() ? q.value(0).toDouble() : 0.0;
    m_months.append(md);

    if (md.revenue > m_maxRevenue)
      m_maxRevenue = md.revenue;
  }

  update();
}

void RevenueChartWidget::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  int w = width();
  int h = height();

  // Background card
  p.setPen(Qt::NoPen);
  p.setBrush(QColor("#0f0f11"));
  p.drawRoundedRect(rect(), 12, 12);

  // Border
  p.setPen(QPen(QColor(255, 255, 255, 10), 1));
  p.setBrush(Qt::NoBrush);
  p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 12, 12);

  // Title
  QFont titleFont("Segoe UI", 13);
  titleFont.setWeight(QFont::DemiBold);
  p.setFont(titleFont);
  p.setPen(QColor("#e4e4e7"));
  p.drawText(24, 20, w - 48, 24, Qt::AlignLeft | Qt::AlignVCenter,
             "Chiffre d'affaires — 6 derniers mois");

  if (m_months.isEmpty())
    return;

  // Chart area
  int chartLeft = 24;
  int chartRight = w - 24;
  int chartTop = 54;
  int chartBottom = h - 36;
  int chartW = chartRight - chartLeft;
  int chartH = chartBottom - chartTop;

  // Grid lines
  p.setPen(QPen(QColor(255, 255, 255, 15), 1, Qt::DotLine));
  for (int i = 0; i <= 4; ++i) {
    int y = chartTop + (chartH * i / 4);
    p.drawLine(chartLeft, y, chartRight, y);
  }

  // Bars
  int barCount = m_months.size();
  double barSpacing = chartW / double(barCount);
  double barW = barSpacing * 0.55;

  double maxVal = m_maxRevenue > 0 ? m_maxRevenue : 1.0;

  // Empty state when no revenue data
  if (m_maxRevenue < 0.01) {
    QFont emptyIconFont("Segoe MDL2 Assets", 28);
    p.setFont(emptyIconFont);
    p.setPen(QColor("#27272a"));
    int emptyY = chartTop + chartH / 2 - 30;
    p.drawText(chartLeft, emptyY, chartW, 36, Qt::AlignCenter,
               QString(QChar(0xE9D9)));

    QFont emptyMsgFont("Segoe UI", 11);
    p.setFont(emptyMsgFont);
    p.setPen(QColor("#3f3f46"));
    p.drawText(chartLeft, emptyY + 42, chartW, 20, Qt::AlignCenter,
               "Aucune donnee de facturation");
  }

  for (int i = 0; i < barCount; ++i) {
    double ratio = m_months[i].revenue / maxVal;
    int barH = int(ratio * chartH);
    int x = chartLeft + int(i * barSpacing + (barSpacing - barW) / 2);
    int y = chartBottom - barH;

    // Gradient bar
    QLinearGradient grad(x, y, x, chartBottom);
    grad.setColorAt(0, QColor("#fbbf24"));
    grad.setColorAt(1, QColor("#d97706"));
    p.setPen(Qt::NoPen);
    p.setBrush(grad);
    p.drawRoundedRect(x, y, int(barW), barH, 4, 4);

    // Value on top
    if (m_months[i].revenue > 0) {
      QFont valFont("Segoe UI", 8);
      valFont.setWeight(QFont::Bold);
      p.setFont(valFont);
      p.setPen(QColor("#fcd34d"));
      QString valStr = QString::number(int(m_months[i].revenue));
      p.drawText(x - 10, y - 16, int(barW) + 20, 14, Qt::AlignCenter,
                 valStr + " EUR");
    }

    // Month label
    QFont labelFont("Segoe UI", 9);
    p.setFont(labelFont);
    p.setPen(QColor("#71717a"));
    p.drawText(x - 10, chartBottom + 4, int(barW) + 20, 18, Qt::AlignCenter,
               m_months[i].label);
  }
}
