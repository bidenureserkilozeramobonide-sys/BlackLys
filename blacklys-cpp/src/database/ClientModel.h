#pragma once

#include <QDateTime>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

struct Client {
  int id = 0;
  QString name;
  QString email;
  QString phone;
  QString company;
  QString address;
  QString notes;
  QString tags; // comma-separated: "vip,prospect,fidele"
  QDateTime createdAt;
  QDateTime updatedAt;
};

class ClientModel {
public:
  static QList<Client> all();
  static Client getById(int id);
  static int create(const Client &client);
  static bool update(const Client &client);
  static bool remove(int id);
  static int count();
  static int countBefore(const QString &date);
  static QList<Client> search(const QString &query);
  static QList<QPair<Client, double>> topByRevenue(int limit = 5);
  static QStringList distinctCompanies();
};
