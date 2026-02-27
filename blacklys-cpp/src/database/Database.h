#pragma once

#include <QSqlDatabase>
#include <QString>

class Database {
public:
  static Database &instance();

  bool initialize(const QString &dbPath = "");
  QSqlDatabase &db();

  // Schema management
  bool createTables();
  int schemaVersion() const;

private:
  Database() = default;
  ~Database();
  Database(const Database &) = delete;
  Database &operator=(const Database &) = delete;

  void migrate(int fromVersion);

  QSqlDatabase m_db;
  static constexpr int CURRENT_SCHEMA_VERSION = 2;
};
