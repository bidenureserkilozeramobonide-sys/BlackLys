#include "MissionModel.h"
#include "Database.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

static Mission missionFromQuery(QSqlQuery &q) {
  Mission m;
  m.id = q.value("id").toInt();
  m.clientId = q.value("client_id").toInt();
  m.title = q.value("title").toString();
  m.address = q.value("address").toString();
  m.date = q.value("date").toString();
  m.time = q.value("time").toString();
  m.status = q.value("status").toString();
  m.type = q.value("type").toString();
  m.notes = q.value("notes").toString();
  m.photoCount = q.value("photo_count").toInt();
  m.createdAt = q.value("created_at").toDateTime();
  m.updatedAt = q.value("updated_at").toDateTime();
  // Joined field (may be null)
  if (q.record().contains("client_name")) {
    m.clientName = q.value("client_name").toString();
  }
  return m;
}

QList<Mission> MissionModel::all() {
  QList<Mission> list;
  QSqlQuery q(Database::instance().db());
  q.exec(R"(
        SELECT m.*, c.name AS client_name
        FROM missions m
        LEFT JOIN clients c ON m.client_id = c.id
        ORDER BY m.date DESC, m.time DESC
    )");
  while (q.next()) {
    list.append(missionFromQuery(q));
  }
  return list;
}

Mission MissionModel::getById(int id) {
  QSqlQuery q(Database::instance().db());
  q.prepare(R"(
        SELECT m.*, c.name AS client_name
        FROM missions m
        LEFT JOIN clients c ON m.client_id = c.id
        WHERE m.id = ?
    )");
  q.addBindValue(id);
  q.exec();
  if (q.next())
    return missionFromQuery(q);
  return {};
}

int MissionModel::create(const Mission &mission) {
  QSqlQuery q(Database::instance().db());
  q.prepare(R"(
        INSERT INTO missions (client_id, title, address, date, time, status, type, notes)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )");
  q.addBindValue(mission.clientId > 0 ? mission.clientId : QVariant());
  q.addBindValue(mission.title);
  q.addBindValue(mission.address);
  q.addBindValue(mission.date);
  q.addBindValue(mission.time);
  q.addBindValue(mission.status.isEmpty() ? "planifiee" : mission.status);
  q.addBindValue(mission.type.isEmpty() ? "photo" : mission.type);
  q.addBindValue(mission.notes);

  if (q.exec())
    return q.lastInsertId().toInt();
  qWarning() << "[MissionModel] create:" << q.lastError().text();
  return -1;
}

bool MissionModel::update(const Mission &mission) {
  QSqlQuery q(Database::instance().db());
  q.prepare(R"(
        UPDATE missions SET
            client_id = ?, title = ?, address = ?, date = ?, time = ?,
            status = ?, type = ?, notes = ?, photo_count = ?,
            updated_at = datetime('now')
        WHERE id = ?
    )");
  q.addBindValue(mission.clientId > 0 ? mission.clientId : QVariant());
  q.addBindValue(mission.title);
  q.addBindValue(mission.address);
  q.addBindValue(mission.date);
  q.addBindValue(mission.time);
  q.addBindValue(mission.status);
  q.addBindValue(mission.type);
  q.addBindValue(mission.notes);
  q.addBindValue(mission.photoCount);
  q.addBindValue(mission.id);

  if (q.exec())
    return true;
  qWarning() << "[MissionModel] update:" << q.lastError().text();
  return false;
}

bool MissionModel::remove(int id) {
  QSqlQuery q(Database::instance().db());
  q.prepare("DELETE FROM missions WHERE id = ?");
  q.addBindValue(id);
  if (q.exec())
    return true;
  qWarning() << "[MissionModel] remove:" << q.lastError().text();
  return false;
}

int MissionModel::count() {
  QSqlQuery q(Database::instance().db());
  q.exec("SELECT COUNT(*) FROM missions");
  if (q.next())
    return q.value(0).toInt();
  return 0;
}

int MissionModel::countBefore(const QString &date) {
  QSqlQuery q(Database::instance().db());
  q.prepare("SELECT COUNT(*) FROM missions WHERE created_at < ?");
  q.addBindValue(date);
  q.exec();
  if (q.next())
    return q.value(0).toInt();
  return 0;
}

int MissionModel::countByStatus(const QString &status) {
  QSqlQuery q(Database::instance().db());
  q.prepare("SELECT COUNT(*) FROM missions WHERE status = ?");
  q.addBindValue(status);
  q.exec();
  if (q.next())
    return q.value(0).toInt();
  return 0;
}

QList<Mission> MissionModel::upcoming(int limit) {
  QList<Mission> list;
  QSqlQuery q(Database::instance().db());
  q.prepare(R"(
        SELECT m.*, c.name AS client_name
        FROM missions m
        LEFT JOIN clients c ON m.client_id = c.id
        WHERE m.date >= date('now') AND m.status IN ('planifiee', 'en_cours')
        ORDER BY m.date ASC, m.time ASC
        LIMIT ?
    )");
  q.addBindValue(limit);
  q.exec();
  while (q.next()) {
    list.append(missionFromQuery(q));
  }
  return list;
}

QList<Mission> MissionModel::byMonth(int year, int month) {
  QList<Mission> list;
  QSqlQuery q(Database::instance().db());
  QString startDate = QString("%1-%2-01")
                          .arg(year, 4, 10, QChar('0'))
                          .arg(month, 2, 10, QChar('0'));
  // Last day of month
  QDate lastDay(year, month, 1);
  lastDay = lastDay.addMonths(1).addDays(-1);
  QString endDate = lastDay.toString("yyyy-MM-dd");

  q.prepare(R"(
        SELECT m.*, c.name AS client_name
        FROM missions m
        LEFT JOIN clients c ON m.client_id = c.id
        WHERE m.date >= ? AND m.date <= ?
        ORDER BY m.date ASC, m.time ASC
    )");
  q.addBindValue(startDate);
  q.addBindValue(endDate);
  q.exec();
  while (q.next()) {
    list.append(missionFromQuery(q));
  }
  return list;
}

QList<Mission> MissionModel::byClientId(int clientId) {
  QList<Mission> list;
  QSqlQuery q(Database::instance().db());
  q.prepare(R"(
        SELECT m.*, c.name AS client_name
        FROM missions m
        LEFT JOIN clients c ON m.client_id = c.id
        WHERE m.client_id = ?
        ORDER BY m.date DESC
    )");
  q.addBindValue(clientId);
  q.exec();
  while (q.next()) {
    list.append(missionFromQuery(q));
  }
  return list;
}

QList<Mission> MissionModel::search(const QString &query) {
  QList<Mission> list;
  QSqlQuery q(Database::instance().db());
  q.prepare(R"(
        SELECT m.*, c.name AS client_name
        FROM missions m
        LEFT JOIN clients c ON m.client_id = c.id
        WHERE m.title LIKE ? OR m.address LIKE ? OR c.name LIKE ?
        ORDER BY m.date DESC
        LIMIT 10
    )");
  QString pattern = "%" + query + "%";
  q.addBindValue(pattern);
  q.addBindValue(pattern);
  q.addBindValue(pattern);
  q.exec();
  while (q.next()) {
    list.append(missionFromQuery(q));
  }
  return list;
}
