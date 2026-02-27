#pragma once

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QTableWidget>
#include <QWidget>

class GalleriesPage : public QWidget {
  Q_OBJECT

public:
  explicit GalleriesPage(QWidget *parent = nullptr);

public slots:
  void refresh();

private slots:
  void onAddGallery();
  void onEditGallery(int row);
  void onDeleteGallery();
  void onViewGallery(int row);
  void onManagePhotos(int galleryId);
  void onTogglePublic(int galleryId);
  void onBatchDelete();
  void onExportCsv();
  void onContextMenu(const QPoint &pos);

private:
  void setupUi();
  void populateTable();
  void populateRecentCards();
  void updateStats();
  QWidget *createStatCard(const QString &icon, const QString &value,
                          const QString &label, const QString &color);
  QWidget *createRecentCard(int id, const QString &title, int photoCount,
                            bool isPublic, const QString &missionTitle);
  static QString relativeTime(const QDateTime &dt);

  QTableWidget *m_table = nullptr;
  QLineEdit *m_searchInput = nullptr;
  QComboBox *m_filterCombo = nullptr;
  QWidget *m_emptyState = nullptr;
  QWidget *m_recentCardsContainer = nullptr;

  // Stat card values
  QLabel *m_statTotalValue = nullptr;
  QLabel *m_statPhotosValue = nullptr;
  QLabel *m_statPublicValue = nullptr;
  QLabel *m_statPrivateValue = nullptr;
};
