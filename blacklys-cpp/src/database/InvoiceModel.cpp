#include "InvoiceModel.h"
#include "Database.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

static Invoice invoiceFromQuery(QSqlQuery &q) {
  Invoice inv;
  inv.id = q.value("id").toInt();
  inv.clientId = q.value("client_id").toInt();
  inv.missionId = q.value("mission_id").toInt();
  inv.number = q.value("number").toString();
  inv.date = q.value("date").toString();
  inv.dueDate = q.value("due_date").toString();
  inv.status = q.value("status").toString();
  inv.subtotal = q.value("subtotal").toDouble();
  inv.taxRate = q.value("tax_rate").toDouble();
  inv.taxAmount = q.value("tax_amount").toDouble();
  inv.total = q.value("total").toDouble();
  inv.notes = q.value("notes").toString();
  inv.createdAt = q.value("created_at").toDateTime();
  inv.updatedAt = q.value("updated_at").toDateTime();
  if (q.record().contains("client_name")) {
    inv.clientName = q.value("client_name").toString();
  }
  return inv;
}

QList<Invoice> InvoiceModel::all() {
  QList<Invoice> list;
  QSqlQuery q(Database::instance().db());
  q.exec(R"(
        SELECT i.*, c.name AS client_name
        FROM invoices i
        LEFT JOIN clients c ON i.client_id = c.id
        ORDER BY i.date DESC
    )");
  while (q.next()) {
    list.append(invoiceFromQuery(q));
  }
  return list;
}

Invoice InvoiceModel::getById(int id) {
  QSqlQuery q(Database::instance().db());
  q.prepare(R"(
        SELECT i.*, c.name AS client_name
        FROM invoices i
        LEFT JOIN clients c ON i.client_id = c.id
        WHERE i.id = ?
    )");
  q.addBindValue(id);
  q.exec();
  if (q.next()) {
    Invoice inv = invoiceFromQuery(q);
    inv.items = getItems(id);
    return inv;
  }
  return {};
}

int InvoiceModel::create(const Invoice &invoice) {
  QSqlQuery q(Database::instance().db());
  q.prepare(R"(
        INSERT INTO invoices (client_id, mission_id, number, date, due_date, status, notes)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )");
  q.addBindValue(invoice.clientId > 0 ? invoice.clientId : QVariant());
  q.addBindValue(invoice.missionId > 0 ? invoice.missionId : QVariant());
  q.addBindValue(invoice.number.isEmpty() ? nextNumber() : invoice.number);
  q.addBindValue(invoice.date);
  q.addBindValue(invoice.dueDate);
  q.addBindValue(invoice.status.isEmpty() ? "brouillon" : invoice.status);
  q.addBindValue(invoice.notes);

  if (q.exec())
    return q.lastInsertId().toInt();
  qWarning() << "[InvoiceModel] create:" << q.lastError().text();
  return -1;
}

bool InvoiceModel::update(const Invoice &invoice) {
  QSqlQuery q(Database::instance().db());
  q.prepare(R"(
        UPDATE invoices SET
            client_id = ?, mission_id = ?, date = ?, due_date = ?,
            status = ?, notes = ?, updated_at = datetime('now')
        WHERE id = ?
    )");
  q.addBindValue(invoice.clientId > 0 ? invoice.clientId : QVariant());
  q.addBindValue(invoice.missionId > 0 ? invoice.missionId : QVariant());
  q.addBindValue(invoice.date);
  q.addBindValue(invoice.dueDate);
  q.addBindValue(invoice.status);
  q.addBindValue(invoice.notes);
  q.addBindValue(invoice.id);

  if (q.exec())
    return true;
  qWarning() << "[InvoiceModel] update:" << q.lastError().text();
  return false;
}

bool InvoiceModel::remove(int id) {
  QSqlQuery q(Database::instance().db());
  q.prepare("DELETE FROM invoices WHERE id = ?");
  q.addBindValue(id);
  if (q.exec())
    return true;
  qWarning() << "[InvoiceModel] remove:" << q.lastError().text();
  return false;
}

int InvoiceModel::count() {
  QSqlQuery q(Database::instance().db());
  q.exec("SELECT COUNT(*) FROM invoices");
  if (q.next())
    return q.value(0).toInt();
  return 0;
}

QString InvoiceModel::nextNumber() {
  QSqlQuery q(Database::instance().db());
  q.exec(
      "SELECT MAX(CAST(REPLACE(number, 'BL-', '') AS INTEGER)) FROM invoices");
  int next = 1;
  if (q.next() && !q.value(0).isNull()) {
    next = q.value(0).toInt() + 1;
  }
  return QString("BL-%1").arg(next, 4, 10, QChar('0'));
}

double InvoiceModel::totalRevenue() {
  QSqlQuery q(Database::instance().db());
  q.exec("SELECT COALESCE(SUM(total), 0) FROM invoices WHERE status = 'payee'");
  if (q.next())
    return q.value(0).toDouble();
  return 0.0;
}

int InvoiceModel::countBefore(const QString &date) {
  QSqlQuery q(Database::instance().db());
  q.prepare("SELECT COUNT(*) FROM invoices WHERE created_at < ?");
  q.addBindValue(date);
  q.exec();
  if (q.next())
    return q.value(0).toInt();
  return 0;
}

int InvoiceModel::countByStatus(const QString &status) {
  QSqlQuery q(Database::instance().db());
  q.prepare("SELECT COUNT(*) FROM invoices WHERE status = ?");
  q.addBindValue(status);
  q.exec();
  if (q.next())
    return q.value(0).toInt();
  return 0;
}

int InvoiceModel::countOverdue() {
  QSqlQuery q(Database::instance().db());
  q.exec("SELECT COUNT(*) FROM invoices "
         "WHERE due_date < date('now') AND status NOT IN ('payee', 'annulee')");
  if (q.next())
    return q.value(0).toInt();
  return 0;
}

double InvoiceModel::totalPending() {
  QSqlQuery q(Database::instance().db());
  q.exec("SELECT COALESCE(SUM(total), 0) FROM invoices "
         "WHERE status NOT IN ('payee', 'annulee')");
  if (q.next())
    return q.value(0).toDouble();
  return 0.0;
}

double InvoiceModel::revenueBefore(const QString &date) {
  QSqlQuery q(Database::instance().db());
  q.prepare("SELECT COALESCE(SUM(total), 0) FROM invoices WHERE status = "
            "'payee' AND created_at < ?");
  q.addBindValue(date);
  q.exec();
  if (q.next())
    return q.value(0).toDouble();
  return 0.0;
}

int InvoiceModel::addItem(const InvoiceItem &item) {
  QSqlQuery q(Database::instance().db());
  double total = item.quantity * item.unitPrice;
  q.prepare(R"(
        INSERT INTO invoice_items (invoice_id, description, quantity, unit_price, total)
        VALUES (?, ?, ?, ?, ?)
    )");
  q.addBindValue(item.invoiceId);
  q.addBindValue(item.description);
  q.addBindValue(item.quantity);
  q.addBindValue(item.unitPrice);
  q.addBindValue(total);

  if (q.exec()) {
    recalculate(item.invoiceId);
    return q.lastInsertId().toInt();
  }
  return -1;
}

bool InvoiceModel::removeItem(int itemId) {
  QSqlQuery q(Database::instance().db());
  // Get invoice_id before deleting
  q.prepare("SELECT invoice_id FROM invoice_items WHERE id = ?");
  q.addBindValue(itemId);
  q.exec();
  int invoiceId = q.next() ? q.value(0).toInt() : 0;

  q.prepare("DELETE FROM invoice_items WHERE id = ?");
  q.addBindValue(itemId);
  if (q.exec() && invoiceId > 0) {
    recalculate(invoiceId);
    return true;
  }
  return false;
}

QList<InvoiceItem> InvoiceModel::getItems(int invoiceId) {
  QList<InvoiceItem> list;
  QSqlQuery q(Database::instance().db());
  q.prepare("SELECT * FROM invoice_items WHERE invoice_id = ?");
  q.addBindValue(invoiceId);
  q.exec();
  while (q.next()) {
    InvoiceItem item;
    item.id = q.value("id").toInt();
    item.invoiceId = q.value("invoice_id").toInt();
    item.description = q.value("description").toString();
    item.quantity = q.value("quantity").toDouble();
    item.unitPrice = q.value("unit_price").toDouble();
    item.total = q.value("total").toDouble();
    list.append(item);
  }
  return list;
}

bool InvoiceModel::recalculate(int invoiceId) {
  QSqlQuery q(Database::instance().db());
  q.prepare(
      "SELECT COALESCE(SUM(total), 0) FROM invoice_items WHERE invoice_id = ?");
  q.addBindValue(invoiceId);
  q.exec();

  double subtotal = q.next() ? q.value(0).toDouble() : 0.0;

  // Get tax rate
  q.prepare("SELECT tax_rate FROM invoices WHERE id = ?");
  q.addBindValue(invoiceId);
  q.exec();
  double taxRate = q.next() ? q.value(0).toDouble() : 20.0;

  double taxAmount = subtotal * taxRate / 100.0;
  double total = subtotal + taxAmount;

  q.prepare(R"(
        UPDATE invoices SET
            subtotal = ?, tax_amount = ?, total = ?,
            updated_at = datetime('now')
        WHERE id = ?
    )");
  q.addBindValue(subtotal);
  q.addBindValue(taxAmount);
  q.addBindValue(total);
  q.addBindValue(invoiceId);

  return q.exec();
}

QList<Invoice> InvoiceModel::byClientId(int clientId) {
  QList<Invoice> list;
  QSqlQuery q(Database::instance().db());
  q.prepare(R"(
        SELECT i.*, c.name AS client_name
        FROM invoices i
        LEFT JOIN clients c ON i.client_id = c.id
        WHERE i.client_id = ?
        ORDER BY i.date DESC
    )");
  q.addBindValue(clientId);
  q.exec();
  while (q.next()) {
    list.append(invoiceFromQuery(q));
  }
  return list;
}

QList<Invoice> InvoiceModel::search(const QString &query) {
  QList<Invoice> list;
  QSqlQuery q(Database::instance().db());
  q.prepare(R"(
        SELECT i.*, c.name AS client_name
        FROM invoices i
        LEFT JOIN clients c ON i.client_id = c.id
        WHERE i.number LIKE ? OR c.name LIKE ? OR i.notes LIKE ?
        ORDER BY i.date DESC
        LIMIT 10
    )");
  QString pattern = "%" + query + "%";
  q.addBindValue(pattern);
  q.addBindValue(pattern);
  q.addBindValue(pattern);
  q.exec();
  while (q.next()) {
    list.append(invoiceFromQuery(q));
  }
  return list;
}
