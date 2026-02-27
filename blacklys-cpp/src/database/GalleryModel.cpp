#include "GalleryModel.h"
#include "Database.h"

#include <QDebug>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QUuid>

static Gallery galleryFromQuery(QSqlQuery &q) {
  Gallery g;
  g.id = q.value("id").toInt();
  g.missionId = q.value("mission_id").toInt();
  g.title = q.value("title").toString();
  g.slug = q.value("slug").toString();
  g.isPublic = q.value("is_public").toInt() != 0;
  g.coverPath = q.value("cover_path").toString();
  g.createdAt = q.value("created_at").toDateTime();
  g.updatedAt = q.value("updated_at").toDateTime();
  if (q.record().contains("mission_title")) {
    g.missionTitle = q.value("mission_title").toString();
  }
  if (q.record().contains("photo_count")) {
    g.photoCount = q.value("photo_count").toInt();
  }
  return g;
}

QList<Gallery> GalleryModel::all() {
  QList<Gallery> list;
  QSqlQuery q(Database::instance().db());
  q.exec(R"(
        SELECT g.*,
               m.title AS mission_title,
               (SELECT COUNT(*) FROM gallery_photos WHERE gallery_id = g.id) AS photo_count
        FROM galleries g
        LEFT JOIN missions m ON g.mission_id = m.id
        ORDER BY g.created_at DESC
    )");
  while (q.next()) {
    list.append(galleryFromQuery(q));
  }
  return list;
}

Gallery GalleryModel::getById(int id) {
  QSqlQuery q(Database::instance().db());
  q.prepare(R"(
        SELECT g.*,
               m.title AS mission_title,
               (SELECT COUNT(*) FROM gallery_photos WHERE gallery_id = g.id) AS photo_count
        FROM galleries g
        LEFT JOIN missions m ON g.mission_id = m.id
        WHERE g.id = ?
    )");
  q.addBindValue(id);
  q.exec();
  if (q.next()) {
    Gallery g = galleryFromQuery(q);
    g.photos = getPhotos(id);
    return g;
  }
  return {};
}

int GalleryModel::create(const Gallery &gallery) {
  QSqlQuery q(Database::instance().db());
  q.prepare(R"(
        INSERT INTO galleries (mission_id, title, slug, is_public, cover_path)
        VALUES (?, ?, ?, ?, ?)
    )");
  q.addBindValue(gallery.missionId > 0 ? gallery.missionId : QVariant());
  q.addBindValue(gallery.title);
  q.addBindValue(gallery.slug.isEmpty() ? generateSlug(gallery.title)
                                        : gallery.slug);
  q.addBindValue(gallery.isPublic ? 1 : 0);
  q.addBindValue(gallery.coverPath);

  if (q.exec())
    return q.lastInsertId().toInt();
  qWarning() << "[GalleryModel] create:" << q.lastError().text();
  return -1;
}

bool GalleryModel::update(const Gallery &gallery) {
  QSqlQuery q(Database::instance().db());
  q.prepare(R"(
        UPDATE galleries SET
            mission_id = ?, title = ?, slug = ?,
            is_public = ?, cover_path = ?,
            updated_at = datetime('now')
        WHERE id = ?
    )");
  q.addBindValue(gallery.missionId > 0 ? gallery.missionId : QVariant());
  q.addBindValue(gallery.title);
  q.addBindValue(gallery.slug);
  q.addBindValue(gallery.isPublic ? 1 : 0);
  q.addBindValue(gallery.coverPath);
  q.addBindValue(gallery.id);

  if (q.exec())
    return true;
  qWarning() << "[GalleryModel] update:" << q.lastError().text();
  return false;
}

bool GalleryModel::remove(int id) {
  QSqlQuery q(Database::instance().db());
  q.prepare("DELETE FROM galleries WHERE id = ?");
  q.addBindValue(id);
  if (q.exec())
    return true;
  qWarning() << "[GalleryModel] remove:" << q.lastError().text();
  return false;
}

int GalleryModel::count() {
  QSqlQuery q(Database::instance().db());
  q.exec("SELECT COUNT(*) FROM galleries");
  if (q.next())
    return q.value(0).toInt();
  return 0;
}

QString GalleryModel::generateSlug(const QString &title) {
  QString slug = title.toLower()
                     .replace(QRegularExpression("[^a-z0-9]+"), "-")
                     .replace(QRegularExpression("^-|-$"), "");
  // Add short UUID suffix for uniqueness
  slug += "-" + QUuid::createUuid().toString(QUuid::Id128).left(6);
  return slug;
}

int GalleryModel::addPhoto(int galleryId, const QString &filePath,
                           int sortOrder) {
  QSqlQuery q(Database::instance().db());
  q.prepare(R"(
        INSERT INTO gallery_photos (gallery_id, file_path, sort_order)
        VALUES (?, ?, ?)
    )");
  q.addBindValue(galleryId);
  q.addBindValue(filePath);
  q.addBindValue(sortOrder);

  if (q.exec())
    return q.lastInsertId().toInt();
  qWarning() << "[GalleryModel] addPhoto:" << q.lastError().text();
  return -1;
}

bool GalleryModel::removePhoto(int photoId) {
  QSqlQuery q(Database::instance().db());
  q.prepare("DELETE FROM gallery_photos WHERE id = ?");
  q.addBindValue(photoId);
  return q.exec();
}

QList<GalleryPhoto> GalleryModel::getPhotos(int galleryId) {
  QList<GalleryPhoto> list;
  QSqlQuery q(Database::instance().db());
  q.prepare(
      "SELECT * FROM gallery_photos WHERE gallery_id = ? ORDER BY sort_order");
  q.addBindValue(galleryId);
  q.exec();
  while (q.next()) {
    GalleryPhoto p;
    p.id = q.value("id").toInt();
    p.galleryId = q.value("gallery_id").toInt();
    p.filePath = q.value("file_path").toString();
    p.sortOrder = q.value("sort_order").toInt();
    p.createdAt = q.value("created_at").toDateTime();
    list.append(p);
  }
  return list;
}

bool GalleryModel::setCover(int galleryId, const QString &coverPath) {
  QSqlQuery q(Database::instance().db());
  q.prepare("UPDATE galleries SET cover_path = ? WHERE id = ?");
  q.addBindValue(coverPath);
  q.addBindValue(galleryId);
  return q.exec();
}

bool GalleryModel::updateSortOrder(int photoId, int newOrder) {
  QSqlQuery q(Database::instance().db());
  q.prepare("UPDATE gallery_photos SET sort_order = ? WHERE id = ?");
  q.addBindValue(newOrder);
  q.addBindValue(photoId);
  return q.exec();
}

QList<Gallery> GalleryModel::byMissionId(int missionId) {
  QList<Gallery> list;
  QSqlQuery q(Database::instance().db());
  q.prepare(R"(
        SELECT g.*,
               m.title AS mission_title,
               (SELECT COUNT(*) FROM gallery_photos WHERE gallery_id = g.id) AS photo_count
        FROM galleries g
        LEFT JOIN missions m ON g.mission_id = m.id
        WHERE g.mission_id = ?
        ORDER BY g.created_at DESC
    )");
  q.addBindValue(missionId);
  q.exec();
  while (q.next()) {
    Gallery g = galleryFromQuery(q);
    g.photos = getPhotos(g.id);
    list.append(g);
  }
  return list;
}

QList<Gallery> GalleryModel::search(const QString &text) {
  QList<Gallery> list;
  QSqlQuery q(Database::instance().db());
  q.prepare(R"(
        SELECT g.*,
               m.title AS mission_title,
               (SELECT COUNT(*) FROM gallery_photos WHERE gallery_id = g.id) AS photo_count
        FROM galleries g
        LEFT JOIN missions m ON g.mission_id = m.id
        WHERE g.title LIKE ? OR m.title LIKE ?
        ORDER BY g.created_at DESC
        LIMIT 10
    )");
  QString pattern = "%" + text + "%";
  q.addBindValue(pattern);
  q.addBindValue(pattern);
  q.exec();
  while (q.next()) {
    list.append(galleryFromQuery(q));
  }
  return list;
}
