#include "MissionCalendarWidget.h"

#include <QLocale>
#include <QMouseEvent>
#include <QPainter>

MissionCalendarWidget::MissionCalendarWidget(QWidget *parent)
    : QWidget(parent) {
  setMinimumHeight(320);
  setObjectName("card");
  QDate today = QDate::currentDate();
  m_year = today.year();
  m_month = today.month();
}

void MissionCalendarWidget::setMonth(int year, int month) {
  m_year = year;
  m_month = month;
  refreshData();
}

void MissionCalendarWidget::refreshData() {
  m_missions = MissionModel::byMonth(m_year, m_month);
  update();
}

void MissionCalendarWidget::setSelectedDate(QDate date) {
  m_selectedDate = date;
  update();
}

QRect MissionCalendarWidget::cellRect(int row, int col) const {
  int w = width();
  int availH = height() - m_headerHeight - m_dayHeaderHeight;
  int cellW = (w - 48) / 7;
  int cellH = availH / 6;
  int x = 24 + col * cellW;
  int y = m_headerHeight + m_dayHeaderHeight + row * cellH;
  return QRect(x, y, cellW, cellH);
}

void MissionCalendarWidget::paintEvent(QPaintEvent *) {
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

  // ── Header: < Month Year >
  QFont titleFont("Segoe UI", 13);
  titleFont.setWeight(QFont::DemiBold);
  p.setFont(titleFont);
  p.setPen(QColor("#e4e4e7"));

  QLocale locale(QLocale::French);
  QString monthName = locale.monthName(m_month, QLocale::LongFormat);
  monthName[0] = monthName[0].toUpper();
  QString headerText = QString("%1 %2").arg(monthName).arg(m_year);

  // Nav arrows
  QFont arrowFont("Segoe MDL2 Assets", 12);

  // Left arrow
  p.setFont(arrowFont);
  p.setPen(QColor("#71717a"));
  p.drawText(24, 12, 36, 28, Qt::AlignCenter, QString(QChar(0xE76B)));

  // Right arrow
  p.drawText(w - 60, 12, 36, 28, Qt::AlignCenter, QString(QChar(0xE76C)));

  // Title centered
  p.setFont(titleFont);
  p.setPen(QColor("#e4e4e7"));
  p.drawText(70, 12, w - 140, 28, Qt::AlignCenter, headerText);

  // ── Day headers (Lun, Mar, Mer, ...)
  QFont dayFont("Segoe UI", 10);
  dayFont.setWeight(QFont::DemiBold);
  p.setFont(dayFont);
  p.setPen(QColor("#52525b"));

  QStringList days = {"Lun", "Mar", "Mer", "Jeu", "Ven", "Sam", "Dim"};
  int cellW = (w - 48) / 7;
  for (int i = 0; i < 7; ++i) {
    int x = 24 + i * cellW;
    p.drawText(x, m_headerHeight, cellW, m_dayHeaderHeight, Qt::AlignCenter,
               days[i]);
  }

  // ── Grid
  QDate firstOfMonth(m_year, m_month, 1);
  int startDow = firstOfMonth.dayOfWeek(); // 1=Mon ... 7=Sun
  int daysInMonth = firstOfMonth.daysInMonth();
  QDate today = QDate::currentDate();

  QFont numFont("Segoe UI", 10);
  p.setFont(numFont);

  // Build mission lookup: day -> list of types
  QMap<int, QStringList> missionsByDay;
  for (const auto &m : m_missions) {
    QDate d = QDate::fromString(m.date, "yyyy-MM-dd");
    if (d.isValid() && d.month() == m_month) {
      missionsByDay[d.day()].append(m.type);
    }
  }

  for (int day = 1; day <= daysInMonth; ++day) {
    int idx = (startDow - 1) + (day - 1);
    int row = idx / 7;
    int col = idx % 7;
    QRect cell = cellRect(row, col);

    bool isToday = (m_year == today.year() && m_month == today.month() &&
                    day == today.day());
    bool isSelected =
        (m_selectedDate.isValid() && m_selectedDate.year() == m_year &&
         m_selectedDate.month() == m_month && m_selectedDate.day() == day);

    // Selected day highlight (amber glow)
    if (isSelected) {
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(245, 158, 11, 40));
      p.drawRoundedRect(cell.adjusted(1, 1, -1, -1), 6, 6);
      // Border
      p.setPen(QPen(QColor(245, 158, 11, 120), 2));
      p.setBrush(Qt::NoBrush);
      p.drawRoundedRect(cell.adjusted(2, 2, -2, -2), 5, 5);
    }
    // Today highlight
    else if (isToday) {
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(99, 102, 241, 30));
      p.drawRoundedRect(cell.adjusted(1, 1, -1, -1), 6, 6);
    }

    // Day number
    QColor textColor = isSelected ? QColor("#fbbf24")
                       : isToday  ? QColor("#fbbf24")
                                  : QColor("#a1a1aa");
    p.setPen(textColor);
    QFont df = numFont;
    if (isToday || isSelected)
      df.setWeight(QFont::Bold);
    p.setFont(df);
    p.drawText(cell.adjusted(6, 4, 0, 0), Qt::AlignLeft | Qt::AlignTop,
               QString::number(day));

    // Mission dots
    if (missionsByDay.contains(day)) {
      const auto &types = missionsByDay[day];
      int dotX = cell.x() + 6;
      int dotY = cell.bottom() - 10;

      for (int i = 0; i < qMin(types.size(), 4); ++i) {
        QColor dotColor("#f59e0b"); // default amber (photo)
        if (types[i] == "video")
          dotColor = QColor("#10b981"); // emerald
        else if (types[i] == "drone")
          dotColor = QColor("#3b82f6"); // blue
        else if (types[i] == "visite_virtuelle")
          dotColor = QColor("#a855f7"); // purple

        p.setPen(Qt::NoPen);
        p.setBrush(dotColor);
        p.drawEllipse(dotX + i * 10, dotY, 6, 6);
      }
    }
  }

  // Grid lines
  p.setPen(QPen(QColor(255, 255, 255, 8), 1));
  int gridTop = m_headerHeight + m_dayHeaderHeight;
  int gridBottom = h;
  int gridLeft = 24;
  int gridRight = 24 + 7 * cellW;

  // Horizontal
  int availH = h - gridTop;
  int cellH = availH / 6;
  for (int i = 0; i <= 6; ++i) {
    int y = gridTop + i * cellH;
    p.drawLine(gridLeft, y, gridRight, y);
  }
  // Vertical
  for (int i = 0; i <= 7; ++i) {
    int x = gridLeft + i * cellW;
    p.drawLine(x, gridTop, x, gridBottom);
  }
}

void MissionCalendarWidget::mousePressEvent(QMouseEvent *event) {
  int w = width();
  int cellW = (w - 48) / 7;

  // Check nav arrows
  QRect leftArrow(24, 12, 36, 28);
  QRect rightArrow(w - 60, 12, 36, 28);

  if (leftArrow.contains(event->pos())) {
    if (m_month == 1) {
      m_month = 12;
      m_year--;
    } else {
      m_month--;
    }
    refreshData();
    return;
  }

  if (rightArrow.contains(event->pos())) {
    if (m_month == 12) {
      m_month = 1;
      m_year++;
    } else {
      m_month++;
    }
    refreshData();
    return;
  }

  // Check day cells
  QDate firstOfMonth(m_year, m_month, 1);
  int startDow = firstOfMonth.dayOfWeek();
  int daysInMonth = firstOfMonth.daysInMonth();

  for (int day = 1; day <= daysInMonth; ++day) {
    int idx = (startDow - 1) + (day - 1);
    int row = idx / 7;
    int col = idx % 7;
    QRect cell = cellRect(row, col);
    if (cell.contains(event->pos())) {
      emit dayClicked(QDate(m_year, m_month, day));
      return;
    }
  }
}
