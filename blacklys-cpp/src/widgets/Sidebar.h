#pragma once

#include <QLabel>
#include <QList>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

class Sidebar : public QWidget {
  Q_OBJECT
  Q_PROPERTY(int activeIndex READ activeIndex WRITE setActiveIndex)
  Q_PROPERTY(int sidebarWidth READ sidebarWidth WRITE setSidebarWidth)

public:
  explicit Sidebar(QWidget *parent = nullptr);
  int activeIndex() const { return m_currentIndex; }
  void setActiveIndex(int index);
  void toggleCollapse();
  int sidebarWidth() const { return width(); }
  void setSidebarWidth(int w) { setFixedWidth(w); }

signals:
  void pageSelected(int index);

private:
  void setupUi();
  void updateActiveIndicator();
  void updateButtonTexts();

  struct NavItem {
    QChar icon;
    QString label;
    QString tooltip;
  };

  QList<QPushButton *> m_buttons;
  QList<QLabel *> m_badges;
  QList<NavItem> m_navItems;
  QWidget *m_indicator = nullptr;
  QLabel *m_brandLabel = nullptr;
  QPushButton *m_collapseBtn = nullptr;
  QPushButton *m_settingsBtn = nullptr;
  int m_currentIndex = -1;
  bool m_collapsed = false;
  bool m_animating = false;
  int m_expandedWidth = 180;
  int m_collapsedWidth = 56;

public:
  void setBadge(int index, int count);
};
