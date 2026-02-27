#include "ClientModel.h"
#include "Database.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

static Client clientFromQuery(QSqlQuery &q) {
  Client c;
  c.id = q.value("id").toInt();
  c.name = q.value("name").toString();
  c.email = q.value("email").toString();
  c.phone = q.value("phone").toString();
  c.company = q.value("company").toString();
  c.address = q.value("address").toString();
  c.notes = q.value("notes").toString();
  c.tags = q.value("tags").toString();
  c.createdAt = q.value("created_at").toDateTime();
  c.updatedAt = q.value("updated_at").toDateTime();
  return c;
}

QList<Client> ClientModel::all() {
  QList<Client> list;
  QSqlQuery q(Database::instance().db());
  q.exec("SELECT * FROM clients ORDER BY name ASC");
  while (q.next()) {
    list.append(clientFromQuery(q));
  }
  return list;
}

Client ClientModel::getById(int id) {
  QSqlQuery q(Database::instance().db());
  q.prepare("SELECT * FROM clients WHERE id = ?");
  q.addBindValue(id);
  q.exec();
  if (q.next()) {
    return clientFromQuery(q);
  }
  return {};
}

int ClientModel::create(const Client &client) {
  QSqlQuery q(Database::instance().db());
  q.prepare(R"(
        INSERT INTO clients (name, email, phone, company, address, notes, tags)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )");
  q.addBindValue(client.name);
  q.addBindValue(client.email);
  q.addBindValue(client.phone);
  q.addBindValue(client.company);
  q.addBindValue(client.address);
  q.addBindValue(client.notes);
  q.addBindValue(client.tags);

  if (q.exec()) {
    return q.lastInsertId().toInt();
  }
  qWarning() << "[ClientModel] create:" << q.lastError().text();
  return -1;
}

bool ClientModel::update(const Client &client) {
  QSqlQuery q(Database::instance().db());
  q.prepare(R"(
        UPDATE clients SET
            name = ?, email = ?, phone = ?, company = ?,
            address = ?, notes = ?, tags = ?, updated_at = datetime('now')
        WHERE id = ?
    )");
  q.addBindValue(client.name);
  q.addBindValue(client.email);
  q.addBindValue(client.phone);
  q.addBindValue(client.company);
  q.addBindValue(client.address);
  q.addBindValue(client.notes);
  q.addBindValue(client.tags);
  q.addBindValue(client.id);

  if (q.exec())
    return true;
  qWarning() << "[ClientModel] update:" << q.lastError().text();
  return false;
}

bool ClientModel::remove(int id) {
  QSqlQuery q(Database::instance().db());
  q.prepare("DELETE FROM clients WHERE id = ?");
  q.addBindValue(id);
  if (q.exec())
    return true;
  qWarning() << "[ClientModel] remove:" << q.lastError().text();
  return false;
}

int ClientModel::count() {
  QSqlQuery q(Database::instance().db());
  q.exec("SELECT COUNT(*) FROM clients");
  if (q.next())
    return q.value(0).toInt();
  return 0;
}

int ClientModel::countBefore(const QString &date) {
  QSqlQuery q(Database::instance().db());
  q.prepare("SELECT COUNT(*) FROM clients WHERE created_at < ?");
  q.addBindValue(date);
  q.exec();
  if (q.next())
    return q.value(0).toInt();
  return 0;
}

QList<Client> ClientModel::search(const QString &query) {
  QList<Client> list;
  QSqlQuery q(Database::instance().db());
  q.prepare(R"(
        SELECT * FROM clients
        WHERE name LIKE ? OR email LIKE ? OR company LIKE ? OR phone LIKE ?
        ORDER BY name ASC
    )");
  QString pattern = "%" + query + "%";
  q.addBindValue(pattern);
  q.addBindValue(pattern);
  q.addBindValue(pattern);
  q.addBindValue(pattern);
  q.exec();
  while (q.next()) {
    list.append(clientFromQuery(q));
  }
  return list;
}

QList<QPair<Client, double>> ClientModel::topByRevenue(int limit) {
  QList<QPair<Client, double>> list;
  QSqlQuery q(Database::instance().db());
  q.prepare(R"(
        SELECT c.*, COALESCE(SUM(i.total), 0) AS revenue
        FROM clients c
        LEFT JOIN invoices i ON c.id = i.client_id
        GROUP BY c.id
        HAVING revenue > 0
        ORDER BY revenue DESC
        LIMIT ?
    )");
  q.addBindValue(limit);
  q.exec();
  while (q.next()) {
    Client c = clientFromQuery(q);
    double revenue = q.value("revenue").toDouble();
    list.append(qMakePair(c, revenue));
  }
  return list;
}

QStringList ClientModel::distinctCompanies() {
  QStringList list;
  QSqlQuery q(Database::instance().db());
  q.exec("SELECT DISTINCT company FROM clients WHERE company != '' ORDER BY "
         "company ASC");
  while (q.next()) {
    list.append(q.value(0).toString());
  }
  return list;
}
