#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

struct Mission {
  int id = 0;
  int clientId = 0;
  QString title;
  QString address;
  QString date;   // ISO format YYYY-MM-DD
  QString time;   // HH:MM
  QString status; // planifiee, en_cours, terminee, annulee
  QString type;   // photo, video, drone, visite_virtuelle
  QString notes;
  int photoCount = 0;
  QDateTime createdAt;
  QDateTime updatedAt;

  // Joined field
  QString clientName;
};

class MissionModel {
public:
  static QList<Mission> all();
  static QList<Mission> byClientId(int clientId);
  static Mission getById(int id);
  static int create(const Mission &mission);
  static bool update(const Mission &mission);
  static bool remove(int id);
  static int count();
  static int countBefore(const QString &date);
  static int countByStatus(const QString &status);
  static QList<Mission> upcoming(int limit = 5);
  static QList<Mission> byMonth(int year, int month);
  static QList<Mission> search(const QString &query);
};
