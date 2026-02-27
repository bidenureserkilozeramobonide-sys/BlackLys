#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

struct InvoiceItem {
  int id = 0;
  int invoiceId = 0;
  QString description;
  double quantity = 1.0;
  double unitPrice = 0.0;
  double total = 0.0;
};

struct Invoice {
  int id = 0;
  int clientId = 0;
  int missionId = 0;
  QString number;
  QString date;
  QString dueDate;
  QString status; // brouillon, envoyee, payee, annulee
  double subtotal = 0.0;
  double taxRate = 20.0;
  double taxAmount = 0.0;
  double total = 0.0;
  QString notes;
  QDateTime createdAt;
  QDateTime updatedAt;

  // Joined
  QString clientName;
  QList<InvoiceItem> items;
};

class InvoiceModel {
public:
  static QList<Invoice> all();
  static Invoice getById(int id);
  static int create(const Invoice &invoice);
  static bool update(const Invoice &invoice);
  static bool remove(int id);
  static int count();
  static int countBefore(const QString &date);
  static int countByStatus(const QString &status);
  static int countOverdue();
  static double totalPending();
  static QString nextNumber();
  static double totalRevenue();
  static double revenueBefore(const QString &date);

  // Items
  static int addItem(const InvoiceItem &item);
  static bool removeItem(int itemId);
  static QList<InvoiceItem> getItems(int invoiceId);
  static bool recalculate(int invoiceId);

  // Queries
  static QList<Invoice> byClientId(int clientId);
  static QList<Invoice> search(const QString &query);
};
