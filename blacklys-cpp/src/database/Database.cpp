#include "Database.h"
#include "ActivityLog.h"

#include <QDebug>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

Database &Database::instance() {
  static Database inst;
  return inst;
}

Database::~Database() {
  if (m_db.isOpen()) {
    m_db.close();
  }
}

bool Database::initialize(const QString &dbPath) {
  QString path = dbPath;
  if (path.isEmpty()) {
    // Default: store in user's AppData
    QString dataDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    path = dataDir + "/blacklys.db";
  }

  qDebug() << "[Database] Opening:" << path;

  m_db = QSqlDatabase::addDatabase("QSQLITE");
  m_db.setDatabaseName(path);

  if (!m_db.open()) {
    qWarning() << "[Database] Failed to open:" << m_db.lastError().text();
    return false;
  }

  // Enable WAL mode for better performance
  QSqlQuery query(m_db);
  query.exec("PRAGMA journal_mode=WAL");
  query.exec("PRAGMA foreign_keys=ON");

  // Create tables if needed
  if (!createTables()) {
    return false;
  }

  // Run migrations if needed
  int version = schemaVersion();
  if (version < CURRENT_SCHEMA_VERSION) {
    migrate(version);
  }

  qDebug() << "[Database] Ready (schema v" << schemaVersion() << ")";
  return true;
}

QSqlDatabase &Database::db() { return m_db; }

int Database::schemaVersion() const {
  QSqlQuery query(m_db);
  query.exec("PRAGMA user_version");
  if (query.next()) {
    return query.value(0).toInt();
  }
  return 0;
}

bool Database::createTables() {
  QSqlQuery query(m_db);

  // ── Clients ──
  bool ok = query.exec(R"(
        CREATE TABLE IF NOT EXISTS clients (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            name        TEXT NOT NULL,
            email       TEXT,
            phone       TEXT,
            company     TEXT,
            address     TEXT,
            notes       TEXT,
            tags        TEXT,
            created_at  TEXT DEFAULT (datetime('now')),
            updated_at  TEXT DEFAULT (datetime('now'))
        )
    )");
  if (!ok) {
    qWarning() << "[Database] clients:" << query.lastError().text();
    return false;
  }

  // ── Missions ──
  ok = query.exec(R"(
        CREATE TABLE IF NOT EXISTS missions (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            client_id   INTEGER REFERENCES clients(id) ON DELETE SET NULL,
            title       TEXT NOT NULL,
            address     TEXT,
            date        TEXT,
            time        TEXT,
            status      TEXT DEFAULT 'planifiee',
            type        TEXT DEFAULT 'photo',
            notes       TEXT,
            photo_count INTEGER DEFAULT 0,
            created_at  TEXT DEFAULT (datetime('now')),
            updated_at  TEXT DEFAULT (datetime('now'))
        )
    )");
  if (!ok) {
    qWarning() << "[Database] missions:" << query.lastError().text();
    return false;
  }

  // ── Galleries ──
  ok = query.exec(R"(
        CREATE TABLE IF NOT EXISTS galleries (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            mission_id  INTEGER REFERENCES missions(id) ON DELETE CASCADE,
            title       TEXT NOT NULL,
            slug        TEXT UNIQUE,
            is_public   INTEGER DEFAULT 0,
            cover_path  TEXT,
            created_at  TEXT DEFAULT (datetime('now')),
            updated_at  TEXT DEFAULT (datetime('now'))
        )
    )");
  if (!ok) {
    qWarning() << "[Database] galleries:" << query.lastError().text();
    return false;
  }

  // ── Gallery Photos ──
  ok = query.exec(R"(
        CREATE TABLE IF NOT EXISTS gallery_photos (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            gallery_id  INTEGER REFERENCES galleries(id) ON DELETE CASCADE,
            file_path   TEXT NOT NULL,
            sort_order  INTEGER DEFAULT 0,
            created_at  TEXT DEFAULT (datetime('now'))
        )
    )");
  if (!ok) {
    qWarning() << "[Database] gallery_photos:" << query.lastError().text();
    return false;
  }

  // ── Invoices ──
  ok = query.exec(R"(
        CREATE TABLE IF NOT EXISTS invoices (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            client_id   INTEGER REFERENCES clients(id) ON DELETE SET NULL,
            mission_id  INTEGER REFERENCES missions(id) ON DELETE SET NULL,
            number      TEXT UNIQUE NOT NULL,
            date        TEXT DEFAULT (date('now')),
            due_date    TEXT,
            status      TEXT DEFAULT 'brouillon',
            subtotal    REAL DEFAULT 0,
            tax_rate    REAL DEFAULT 20.0,
            tax_amount  REAL DEFAULT 0,
            total       REAL DEFAULT 0,
            notes       TEXT,
            created_at  TEXT DEFAULT (datetime('now')),
            updated_at  TEXT DEFAULT (datetime('now'))
        )
    )");
  if (!ok) {
    qWarning() << "[Database] invoices:" << query.lastError().text();
    return false;
  }

  // ── Invoice Items ──
  ok = query.exec(R"(
        CREATE TABLE IF NOT EXISTS invoice_items (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            invoice_id  INTEGER REFERENCES invoices(id) ON DELETE CASCADE,
            description TEXT NOT NULL,
            quantity    REAL DEFAULT 1,
            unit_price  REAL DEFAULT 0,
            total       REAL DEFAULT 0
        )
    )");
  if (!ok) {
    qWarning() << "[Database] invoice_items:" << query.lastError().text();
    return false;
  }

  // ── Settings ──
  ok = query.exec(R"(
        CREATE TABLE IF NOT EXISTS settings (
            key         TEXT PRIMARY KEY,
            value       TEXT
        )
    )");
  if (!ok) {
    qWarning() << "[Database] settings:" << query.lastError().text();
    return false;
  }

  // ── Quotes ──
  ok = query.exec(R"(
        CREATE TABLE IF NOT EXISTS quotes (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            client_id   INTEGER REFERENCES clients(id) ON DELETE SET NULL,
            mission_id  INTEGER REFERENCES missions(id) ON DELETE SET NULL,
            number      TEXT UNIQUE NOT NULL,
            date        TEXT DEFAULT (date('now')),
            valid_until TEXT,
            status      TEXT DEFAULT 'brouillon',
            subtotal    REAL DEFAULT 0,
            tax_rate    REAL DEFAULT 20.0,
            tax_amount  REAL DEFAULT 0,
            total       REAL DEFAULT 0,
            notes       TEXT,
            created_at  TEXT DEFAULT (datetime('now')),
            updated_at  TEXT DEFAULT (datetime('now'))
        )
    )");
  if (!ok) {
    qWarning() << "[Database] quotes:" << query.lastError().text();
    return false;
  }

  // ── Quote Items ──
  ok = query.exec(R"(
        CREATE TABLE IF NOT EXISTS quote_items (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            quote_id    INTEGER REFERENCES quotes(id) ON DELETE CASCADE,
            description TEXT NOT NULL,
            quantity    REAL DEFAULT 1,
            unit_price  REAL DEFAULT 0,
            total       REAL DEFAULT 0
        )
    )");
  if (!ok) {
    qWarning() << "[Database] quote_items:" << query.lastError().text();
    return false;
  }

  // ── Quote Templates ──
  ok = query.exec(R"(
        CREATE TABLE IF NOT EXISTS quote_templates (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            name        TEXT NOT NULL,
            notes       TEXT,
            created_at  TEXT DEFAULT (datetime('now'))
        )
    )");
  if (!ok) {
    qWarning() << "[Database] quote_templates:" << query.lastError().text();
    return false;
  }

  // ── Quote Template Items ──
  ok = query.exec(R"(
        CREATE TABLE IF NOT EXISTS quote_template_items (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            template_id   INTEGER REFERENCES quote_templates(id) ON DELETE CASCADE,
            description   TEXT NOT NULL,
            quantity      REAL DEFAULT 1,
            unit_price    REAL DEFAULT 0,
            discount      REAL DEFAULT 0
        )
    )");
  if (!ok) {
    qWarning() << "[Database] quote_template_items:"
               << query.lastError().text();
    return false;
  }

  // Set schema version (only on fresh creation; migrations update it
  // themselves)
  if (schemaVersion() == 0) {
    query.exec(QString("PRAGMA user_version = %1").arg(CURRENT_SCHEMA_VERSION));
  }

  // ── Activity Log ──
  ActivityLog::createTable();

  // ── Indices for query performance ──
  query.exec(
      "CREATE INDEX IF NOT EXISTS idx_missions_status ON missions(status)");
  query.exec("CREATE INDEX IF NOT EXISTS idx_missions_date ON missions(date)");
  query.exec(
      "CREATE INDEX IF NOT EXISTS idx_missions_client ON missions(client_id)");
  query.exec(
      "CREATE INDEX IF NOT EXISTS idx_invoices_status ON invoices(status)");
  query.exec(
      "CREATE INDEX IF NOT EXISTS idx_invoices_client ON invoices(client_id)");
  query.exec(
      "CREATE INDEX IF NOT EXISTS idx_invoices_due_date ON invoices(due_date)");
  query.exec("CREATE INDEX IF NOT EXISTS idx_quotes_status ON quotes(status)");
  query.exec(
      "CREATE INDEX IF NOT EXISTS idx_quotes_client ON quotes(client_id)");
  query.exec("CREATE INDEX IF NOT EXISTS idx_gallery_photos_gallery ON "
             "gallery_photos(gallery_id)");
  query.exec(
      "CREATE INDEX IF NOT EXISTS idx_clients_created ON clients(created_at)");

  return true;
}

void Database::migrate(int fromVersion) {
  QSqlQuery q(m_db);

  if (fromVersion < 2) {
    // Add tags column to clients
    q.exec("ALTER TABLE clients ADD COLUMN tags TEXT");
    // Add discount column to quote_items
    q.exec("ALTER TABLE quote_items ADD COLUMN discount REAL DEFAULT 0");
    // Add discount column to invoice_items
    q.exec("ALTER TABLE invoice_items ADD COLUMN discount REAL DEFAULT 0");
    // Create templates tables
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS quote_templates (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            name        TEXT NOT NULL,
            notes       TEXT,
            created_at  TEXT DEFAULT (datetime('now'))
        )
    )");
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS quote_template_items (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            template_id   INTEGER REFERENCES quote_templates(id) ON DELETE CASCADE,
            description   TEXT NOT NULL,
            quantity      REAL DEFAULT 1,
            unit_price    REAL DEFAULT 0,
            discount      REAL DEFAULT 0
        )
    )");
    q.exec("PRAGMA user_version = 2");
    qDebug() << "[Database] Migration v1→v2: added clients.tags, discount "
                "columns, and template tables";
  }

  q.exec(QString("PRAGMA user_version = %1").arg(CURRENT_SCHEMA_VERSION));
}
