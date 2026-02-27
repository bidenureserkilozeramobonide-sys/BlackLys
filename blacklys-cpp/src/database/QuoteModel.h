#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

struct QuoteItem {
  int id = 0;
  int quoteId = 0;
  QString description;
  double quantity = 1.0;
  double unitPrice = 0.0;
  double discount = 0.0; // percentage 0-100
  double total = 0.0;
};

struct Quote {
  int id = 0;
  int clientId = 0;
  int missionId = 0;
  QString number;
  QString date;
  QString validUntil;
  QString status; // brouillon, envoyee, acceptee, refusee
  double subtotal = 0.0;
  double taxRate = 20.0;
  double taxAmount = 0.0;
  double total = 0.0;
  QString notes;
  QDateTime createdAt;
  QDateTime updatedAt;

  // Joined
  QString clientName;
  QList<QuoteItem> items;
};

// ── Templates ──

struct QuoteTemplateItem {
  int id = 0;
  int templateId = 0;
  QString description;
  double quantity = 1.0;
  double unitPrice = 0.0;
  double discount = 0.0;
};

struct QuoteTemplate {
  int id = 0;
  QString name;
  QString notes;
  QDateTime createdAt;
  QList<QuoteTemplateItem> items;
};

class QuoteModel {
public:
  static QList<Quote> all();
  static Quote getById(int id);
  static int create(const Quote &quote);
  static bool update(const Quote &quote);
  static bool remove(int id);
  static int count();
  static int countBefore(const QString &date);
  static int countByStatus(const QString &status);
  static QList<Quote> search(const QString &text);
  static QString nextNumber();

  // Items
  static int addItem(const QuoteItem &item);
  static bool removeItem(int itemId);
  static QList<QuoteItem> getItems(int quoteId);
  static bool recalculate(int quoteId);

  // Conversion
  static int convertToInvoice(int quoteId);

  // Queries
  static QList<Quote> byClientId(int clientId);

  // Templates
  static QList<QuoteTemplate> allTemplates();
  static QuoteTemplate getTemplate(int templateId);
  static int createTemplate(const QuoteTemplate &tpl);
  static bool removeTemplate(int templateId);
  static int addTemplateItem(const QuoteTemplateItem &item);
};
