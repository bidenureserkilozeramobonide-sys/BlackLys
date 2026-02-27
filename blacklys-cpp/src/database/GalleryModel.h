#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

struct GalleryPhoto {
  int id = 0;
  int galleryId = 0;
  QString filePath;
  int sortOrder = 0;
  QDateTime createdAt;
};

struct Gallery {
  int id = 0;
  int missionId = 0;
  QString title;
  QString slug;
  bool isPublic = false;
  QString coverPath;
  QDateTime createdAt;
  QDateTime updatedAt;

  // Joined
  QString missionTitle;
  int photoCount = 0;
  QList<GalleryPhoto> photos;
};

class GalleryModel {
public:
  static QList<Gallery> all();
  static Gallery getById(int id);
  static int create(const Gallery &gallery);
  static bool update(const Gallery &gallery);
  static bool remove(int id);
  static int count();
  static QList<Gallery> search(const QString &text);
  static QString generateSlug(const QString &title);

  // Photos
  static int addPhoto(int galleryId, const QString &filePath,
                      int sortOrder = 0);
  static bool removePhoto(int photoId);
  static QList<GalleryPhoto> getPhotos(int galleryId);
  static bool setCover(int galleryId, const QString &coverPath);
  static bool updateSortOrder(int photoId, int newOrder);
  static QList<Gallery> byMissionId(int missionId);
};
