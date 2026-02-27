#include "ActivityLog.h"
#include "Database.h"
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

void ActivityLog::createTable() {
  QSqlQuery q(Database::instance().db());
  q.exec("CREATE TABLE IF NOT EXISTS activity_log ("
         "id INTEGER PRIMARY KEY AUTOINCREMENT,"
         "action TEXT NOT NULL,"
         "entity TEXT NOT NULL,"
         "detail TEXT,"
         "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP)");
}

void ActivityLog::log(const QString &action, const QString &entity,
                      const QString &detail) {
  QSqlQuery q(Database::instance().db());
  q.prepare("INSERT INTO activity_log (action, entity, detail) "
            "VALUES (:action, :entity, :detail)");
  q.bindValue(":action", action);
  q.bindValue(":entity", entity);
  q.bindValue(":detail", detail);
  q.exec();
}

QList<Activity> ActivityLog::recent(int limit) {
  QList<Activity> list;
  QSqlQuery q(Database::instance().db());
  q.prepare("SELECT id, action, entity, detail, timestamp "
            "FROM activity_log ORDER BY timestamp DESC LIMIT :limit");
  q.bindValue(":limit", limit);
  q.exec();
  while (q.next()) {
    Activity a;
    a.id = q.value(0).toInt();
    a.action = q.value(1).toString();
    a.entity = q.value(2).toString();
    a.detail = q.value(3).toString();
    a.timestamp = q.value(4).toDateTime();
    list.append(a);
  }
  return list;
}

int ActivityLog::recentCount(int hours) {
  QSqlQuery q(Database::instance().db());
  q.prepare("SELECT COUNT(*) FROM activity_log "
            "WHERE timestamp > datetime('now', '-' || :hours || ' hours')");
  q.bindValue(":hours", hours);
  q.exec();
  if (q.next())
    return q.value(0).toInt();
  return 0;
}
