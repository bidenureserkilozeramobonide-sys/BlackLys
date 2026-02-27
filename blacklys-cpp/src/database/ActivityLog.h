#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

struct Activity {
  int id = 0;
  QString action; // "create", "edit", "delete"
  QString entity; // "client", "mission", "facture"
  QString detail; // e.g. "Client: Jean Dupont"
  QDateTime timestamp;
};

class ActivityLog {
public:
  static void createTable();
  static void log(const QString &action, const QString &entity,
                  const QString &detail);
  static QList<Activity> recent(int limit = 10);
  static int recentCount(int hours = 24);
};
