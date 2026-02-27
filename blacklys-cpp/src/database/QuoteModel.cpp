#include "QuoteModel.h"
#include "Database.h"
#include "InvoiceModel.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>

static Quote quoteFromQuery(QSqlQuery &q) {
  Quote qu;
  qu.id = q.value("id").toInt();
  qu.clientId = q.value("client_id").toInt();
  qu.missionId = q.value("mission_id").toInt();
  qu.number = q.value("number").toString();
  qu.date = q.value("date").toString();
  qu.validUntil = q.value("valid_until").toString();
  qu.status = q.value("status").toString();
  qu.subtotal = q.value("subtotal").toDouble();
  qu.taxRate = q.value("tax_rate").toDouble();
  qu.taxAmount = q.value("tax_amount").toDouble();
  qu.total = q.value("total").toDouble();
  qu.notes = q.value("notes").toString();
  qu.createdAt = q.value("created_at").toDateTime();
  qu.updatedAt = q.value("updated_at").toDateTime();
  if (q.record().contains("client_name")) {
    qu.clientName = q.value("client_name").toString();
  }
  return qu;
}

QList<Quote> QuoteModel::all() {
  QList<Quote> list;
  QSqlQuery q(Database::instance().db());
  q.exec(R"(
        SELECT q.*, c.name AS client_name
        FROM quotes q
        LEFT JOIN clients c ON q.client_id = c.id
        ORDER BY q.date DESC
    )");
  while (q.next()) {
    list.append(quoteFromQuery(q));
  }
  return list;
}

Quote QuoteModel::getById(int id) {
  QSqlQuery q(Database::instance().db());
  q.prepare(R"(
        SELECT q.*, c.name AS client_name
        FROM quotes q
        LEFT JOIN clients c ON q.client_id = c.id
        WHERE q.id = ?
    )");
  q.addBindValue(id);
  q.exec();
  if (q.next()) {
    Quote qu = quoteFromQuery(q);
    qu.items = getItems(id);
    return qu;
  }
  return {};
}

int QuoteModel::create(const Quote &quote) {
  QSqlQuery q(Database::instance().db());
  q.prepare(R"(
        INSERT INTO quotes (client_id, mission_id, number, date, valid_until,
                            status, tax_rate, notes)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )");
  q.addBindValue(quote.clientId > 0 ? quote.clientId : QVariant());
  q.addBindValue(quote.missionId > 0 ? quote.missionId : QVariant());
  q.addBindValue(quote.number.isEmpty() ? nextNumber() : quote.number);
  q.addBindValue(quote.date);
  q.addBindValue(quote.validUntil);
  q.addBindValue(quote.status.isEmpty() ? "brouillon" : quote.status);
  q.addBindValue(quote.taxRate);
  q.addBindValue(quote.notes);

  if (q.exec())
    return q.lastInsertId().toInt();
  qWarning() << "[QuoteModel] create:" << q.lastError().text();
  return -1;
}

bool QuoteModel::update(const Quote &quote) {
  QSqlQuery q(Database::instance().db());
  q.prepare(R"(
        UPDATE quotes SET
            client_id = ?, mission_id = ?, date = ?, valid_until = ?,
            status = ?, tax_rate = ?, notes = ?, updated_at = datetime('now')
        WHERE id = ?
    )");
  q.addBindValue(quote.clientId > 0 ? quote.clientId : QVariant());
  q.addBindValue(quote.missionId > 0 ? quote.missionId : QVariant());
  q.addBindValue(quote.date);
  q.addBindValue(quote.validUntil);
  q.addBindValue(quote.status);
  q.addBindValue(quote.taxRate);
  q.addBindValue(quote.notes);
  q.addBindValue(quote.id);

  if (q.exec())
    return true;
  qWarning() << "[QuoteModel] update:" << q.lastError().text();
  return false;
}

bool QuoteModel::remove(int id) {
  QSqlQuery q(Database::instance().db());
  q.prepare("DELETE FROM quotes WHERE id = ?");
  q.addBindValue(id);
  if (q.exec())
    return true;
  qWarning() << "[QuoteModel] remove:" << q.lastError().text();
  return false;
}

int QuoteModel::count() {
  QSqlQuery q(Database::instance().db());
  q.exec("SELECT COUNT(*) FROM quotes");
  if (q.next())
    return q.value(0).toInt();
  return 0;
}

int QuoteModel::countBefore(const QString &date) {
  QSqlQuery q(Database::instance().db());
  q.prepare("SELECT COUNT(*) FROM quotes WHERE created_at < :date");
  q.bindValue(":date", date);
  q.exec();
  if (q.next())
    return q.value(0).toInt();
  return 0;
}

int QuoteModel::countByStatus(const QString &status) {
  QSqlQuery q(Database::instance().db());
  q.prepare("SELECT COUNT(*) FROM quotes WHERE status = ?");
  q.addBindValue(status);
  q.exec();
  if (q.next())
    return q.value(0).toInt();
  return 0;
}

QString QuoteModel::nextNumber() {
  QSqlQuery q(Database::instance().db());
  q.exec(
      "SELECT MAX(CAST(REPLACE(number, 'DEV-', '') AS INTEGER)) FROM quotes");
  int next = 1;
  if (q.next() && !q.value(0).isNull()) {
    next = q.value(0).toInt() + 1;
  }
  return QString("DEV-%1").arg(next, 4, 10, QChar('0'));
}

// ════════════════════════════════════════════════
//  Items (with discount)
// ════════════════════════════════════════════════

int QuoteModel::addItem(const QuoteItem &item) {
  QSqlQuery q(Database::instance().db());
  double total = item.quantity * item.unitPrice * (1.0 - item.discount / 100.0);
  q.prepare(R"(
        INSERT INTO quote_items (quote_id, description, quantity, unit_price, discount, total)
        VALUES (?, ?, ?, ?, ?, ?)
    )");
  q.addBindValue(item.quoteId);
  q.addBindValue(item.description);
  q.addBindValue(item.quantity);
  q.addBindValue(item.unitPrice);
  q.addBindValue(item.discount);
  q.addBindValue(total);

  if (q.exec()) {
    recalculate(item.quoteId);
    return q.lastInsertId().toInt();
  }
  return -1;
}

bool QuoteModel::removeItem(int itemId) {
  QSqlQuery q(Database::instance().db());
  q.prepare("SELECT quote_id FROM quote_items WHERE id = ?");
  q.addBindValue(itemId);
  q.exec();
  int quoteId = q.next() ? q.value(0).toInt() : 0;

  q.prepare("DELETE FROM quote_items WHERE id = ?");
  q.addBindValue(itemId);
  if (q.exec() && quoteId > 0) {
    recalculate(quoteId);
    return true;
  }
  return false;
}

QList<QuoteItem> QuoteModel::getItems(int quoteId) {
  QList<QuoteItem> list;
  QSqlQuery q(Database::instance().db());
  q.prepare("SELECT * FROM quote_items WHERE quote_id = ?");
  q.addBindValue(quoteId);
  q.exec();
  while (q.next()) {
    QuoteItem item;
    item.id = q.value("id").toInt();
    item.quoteId = q.value("quote_id").toInt();
    item.description = q.value("description").toString();
    item.quantity = q.value("quantity").toDouble();
    item.unitPrice = q.value("unit_price").toDouble();
    item.discount = q.value("discount").toDouble();
    item.total = q.value("total").toDouble();
    list.append(item);
  }
  return list;
}

bool QuoteModel::recalculate(int quoteId) {
  QSqlQuery q(Database::instance().db());
  q.prepare(
      "SELECT COALESCE(SUM(total), 0) FROM quote_items WHERE quote_id = ?");
  q.addBindValue(quoteId);
  q.exec();

  double subtotal = q.next() ? q.value(0).toDouble() : 0.0;

  q.prepare("SELECT tax_rate FROM quotes WHERE id = ?");
  q.addBindValue(quoteId);
  q.exec();
  double taxRate = q.next() ? q.value(0).toDouble() : 20.0;

  double taxAmount = subtotal * taxRate / 100.0;
  double total = subtotal + taxAmount;

  q.prepare(R"(
        UPDATE quotes SET
            subtotal = ?, tax_amount = ?, total = ?,
            updated_at = datetime('now')
        WHERE id = ?
    )");
  q.addBindValue(subtotal);
  q.addBindValue(taxAmount);
  q.addBindValue(total);
  q.addBindValue(quoteId);

  return q.exec();
}

int QuoteModel::convertToInvoice(int quoteId) {
  Quote quote = getById(quoteId);
  if (quote.id == 0)
    return -1;

  // Create invoice from quote
  Invoice inv;
  inv.clientId = quote.clientId;
  inv.missionId = quote.missionId;
  inv.date = QDate::currentDate().toString("yyyy-MM-dd");
  inv.dueDate = QDate::currentDate().addDays(30).toString("yyyy-MM-dd");
  inv.status = "brouillon";
  inv.notes = QString("Converti depuis devis %1").arg(quote.number);

  int invoiceId = InvoiceModel::create(inv);
  if (invoiceId < 0)
    return -1;

  // Copy items
  for (const auto &qi : quote.items) {
    InvoiceItem ii;
    ii.invoiceId = invoiceId;
    ii.description = qi.description;
    ii.quantity = qi.quantity;
    ii.unitPrice = qi.unitPrice;
    InvoiceModel::addItem(ii);
  }

  // Mark quote as accepted
  quote.status = "acceptee";
  update(quote);

  return invoiceId;
}

QList<Quote> QuoteModel::byClientId(int clientId) {
  QList<Quote> list;
  QSqlQuery q(Database::instance().db());
  q.prepare(R"(
        SELECT q.*, c.name AS client_name
        FROM quotes q
        LEFT JOIN clients c ON q.client_id = c.id
        WHERE q.client_id = ?
        ORDER BY q.date DESC
    )");
  q.addBindValue(clientId);
  q.exec();
  while (q.next()) {
    list.append(quoteFromQuery(q));
  }
  return list;
}

QList<Quote> QuoteModel::search(const QString &text) {
  QList<Quote> list;
  QSqlQuery q(Database::instance().db());
  q.prepare(R"(
        SELECT q.*, c.name AS client_name
        FROM quotes q
        LEFT JOIN clients c ON q.client_id = c.id
        WHERE q.number LIKE ? OR c.name LIKE ?
        ORDER BY q.date DESC
        LIMIT 10
    )");
  QString pattern = "%" + text + "%";
  q.addBindValue(pattern);
  q.addBindValue(pattern);
  q.exec();
  while (q.next()) {
    list.append(quoteFromQuery(q));
  }
  return list;
}

// ════════════════════════════════════════════════
//  Templates
// ════════════════════════════════════════════════

QList<QuoteTemplate> QuoteModel::allTemplates() {
  QList<QuoteTemplate> list;
  QSqlQuery q(Database::instance().db());
  q.exec("SELECT * FROM quote_templates ORDER BY name");
  while (q.next()) {
    QuoteTemplate tpl;
    tpl.id = q.value("id").toInt();
    tpl.name = q.value("name").toString();
    tpl.notes = q.value("notes").toString();
    tpl.createdAt = q.value("created_at").toDateTime();
    list.append(tpl);
  }
  return list;
}

QuoteTemplate QuoteModel::getTemplate(int templateId) {
  QSqlQuery q(Database::instance().db());
  q.prepare("SELECT * FROM quote_templates WHERE id = ?");
  q.addBindValue(templateId);
  q.exec();
  QuoteTemplate tpl;
  if (q.next()) {
    tpl.id = q.value("id").toInt();
    tpl.name = q.value("name").toString();
    tpl.notes = q.value("notes").toString();
    tpl.createdAt = q.value("created_at").toDateTime();

    // Load items
    QSqlQuery qi(Database::instance().db());
    qi.prepare("SELECT * FROM quote_template_items WHERE template_id = ?");
    qi.addBindValue(templateId);
    qi.exec();
    while (qi.next()) {
      QuoteTemplateItem item;
      item.id = qi.value("id").toInt();
      item.templateId = qi.value("template_id").toInt();
      item.description = qi.value("description").toString();
      item.quantity = qi.value("quantity").toDouble();
      item.unitPrice = qi.value("unit_price").toDouble();
      item.discount = qi.value("discount").toDouble();
      tpl.items.append(item);
    }
  }
  return tpl;
}

int QuoteModel::createTemplate(const QuoteTemplate &tpl) {
  QSqlQuery q(Database::instance().db());
  q.prepare("INSERT INTO quote_templates (name, notes) VALUES (?, ?)");
  q.addBindValue(tpl.name);
  q.addBindValue(tpl.notes);
  if (q.exec()) {
    int id = q.lastInsertId().toInt();
    for (const auto &item : tpl.items) {
      QuoteTemplateItem ti = item;
      ti.templateId = id;
      addTemplateItem(ti);
    }
    return id;
  }
  return -1;
}

bool QuoteModel::removeTemplate(int templateId) {
  QSqlQuery q(Database::instance().db());
  q.prepare("DELETE FROM quote_templates WHERE id = ?");
  q.addBindValue(templateId);
  return q.exec();
}

int QuoteModel::addTemplateItem(const QuoteTemplateItem &item) {
  QSqlQuery q(Database::instance().db());
  q.prepare(R"(
        INSERT INTO quote_template_items
        (template_id, description, quantity, unit_price, discount)
        VALUES (?, ?, ?, ?, ?)
    )");
  q.addBindValue(item.templateId);
  q.addBindValue(item.description);
  q.addBindValue(item.quantity);
  q.addBindValue(item.unitPrice);
  q.addBindValue(item.discount);
  if (q.exec())
    return q.lastInsertId().toInt();
  return -1;
}
