#pragma once

#include <QMainWindow>
#include <QStackedWidget>

class QLabel;
class QPropertyAnimation;
class QGraphicsOpacityEffect;

class Sidebar;
class StatusBarWidget;
class SearchOverlay;

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override = default;

  // Fullscreen editing mode
  void toggleFullscreen();
  bool isEditingFullscreen() const { return m_isFullscreen; }

private slots:
  void navigateTo(int index);

private:
  void setupUi();
  void createPages();

  Sidebar *m_sidebar = nullptr;
  StatusBarWidget *m_statusBar = nullptr;
  SearchOverlay *m_searchOverlay = nullptr;
  QStackedWidget *m_pages = nullptr;
  bool m_isFullscreen = false;

  // Page transition animation
  bool m_transitioning = false;

  static constexpr int PAGE_DASHBOARD = 0;
  static constexpr int PAGE_CLIENTS = 1;
  static constexpr int PAGE_DEVIS = 2;
  static constexpr int PAGE_MISSIONS = 3;
  static constexpr int PAGE_HDR_STUDIO = 4;
  static constexpr int PAGE_GALLERIES = 5;
  static constexpr int PAGE_BILLING = 6;
  static constexpr int PAGE_SETTINGS = 7;
};
