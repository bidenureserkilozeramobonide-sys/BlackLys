#include "DevisPage.h"
#include "../database/ActivityLog.h"
#include "database/ClientModel.h"
#include "database/Database.h"
#include "database/MissionModel.h"
#include "database/QuoteModel.h"
#include "widgets/PageHeader.h"
#include "widgets/ToastWidget.h"


#include <QApplication>
#include <QComboBox>
#include <QDateEdit>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPdfWriter>
#include <QPushButton>
#include <QShortcut>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStyle>
#include <QTableWidget>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>

// ════════════════════════════════════════════════
//  Quick client creation mini-dialog
// ════════════════════════════════════════════════
static int quickCreateClient(QWidget *parent) {
  QDialog dlg(parent);
  dlg.setWindowTitle("Nouveau client rapide");
  dlg.setMinimumWidth(360);
  dlg.setStyleSheet(
      "QDialog{background:#09090b;color:#e4e4e7;}"
      "QLabel{color:#a1a1aa;background:transparent;}"
      "QLineEdit{background:#18181b;color:#e4e4e7;border:1px solid #27272a;"
      "border-radius:6px;padding:6px;}"
      "QLineEdit:focus{border-color:#f59e0b;}");
  auto *lay = new QFormLayout(&dlg);
  auto *nameEdit = new QLineEdit(&dlg);
  auto *emailEdit = new QLineEdit(&dlg);
  lay->addRow("Nom *", nameEdit);
  lay->addRow("Email", emailEdit);
  auto *btns =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  btns->button(QDialogButtonBox::Ok)
      ->setStyleSheet("QPushButton{background:#f59e0b;color:white;border:none;"
                      "border-radius:6px;padding:8px 20px;font-weight:600;}"
                      "QPushButton:hover{background:#fbbf24;}");
  btns->button(QDialogButtonBox::Cancel)
      ->setStyleSheet(
          "QPushButton{background:#27272a;color:#a1a1aa;border:none;"
          "border-radius:6px;padding:8px 20px;}"
          "QPushButton:hover{background:#3f3f46;color:white;}");
  QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  lay->addRow(btns);
  if (dlg.exec() != QDialog::Accepted || nameEdit->text().trimmed().isEmpty())
    return 0;
  Client c;
  c.name = nameEdit->text().trimmed();
  c.email = emailEdit->text().trimmed();
  return ClientModel::create(c);
}

// ════════════════════════════════════════════════
//  Quote Dialog (unified add/edit/duplicate)
// ════════════════════════════════════════════════
static bool showQuoteDialog(QWidget *parent, Quote &quote, bool isNew) {
  QDialog dlg(parent);
  dlg.setWindowTitle(isNew ? "Nouveau devis" : "Modifier le devis");
  dlg.setMinimumSize(680, 620);
  dlg.setStyleSheet(
      "QDialog{background:#09090b;color:#e4e4e7;}"
      "QLabel{color:#a1a1aa;background:transparent;}"
      "QLineEdit,QComboBox,QDateEdit,QDoubleSpinBox,QTextEdit{"
      "background:#18181b;color:#e4e4e7;border:1px solid #27272a;"
      "border-radius:6px;padding:6px;}"
      "QLineEdit:focus,QComboBox:focus,QDateEdit:focus{border-color:#f59e0b;}");
  auto *layout = new QVBoxLayout(&dlg);
  auto *form = new QFormLayout();
  form->setSpacing(10);

  // Number
  auto *numberEdit =
      new QLineEdit(isNew ? QuoteModel::nextNumber() : quote.number, &dlg);
  numberEdit->setReadOnly(true);
  form->addRow("Numero", numberEdit);

  // Client + quick create "+"
  auto *clientRow = new QWidget(&dlg);
  auto *clientLay = new QHBoxLayout(clientRow);
  clientLay->setContentsMargins(0, 0, 0, 0);
  clientLay->setSpacing(4);
  auto *clientCombo = new QComboBox(clientRow);
  clientCombo->addItem(QString::fromUtf8("\xe2\x80\x94 Aucun \xe2\x80\x94"), 0);
  auto clients = ClientModel::all();
  for (const auto &c : clients)
    clientCombo->addItem(c.name, c.id);
  for (int i = 0; i < clientCombo->count(); ++i)
    if (clientCombo->itemData(i).toInt() == quote.clientId) {
      clientCombo->setCurrentIndex(i);
      break;
    }
  clientLay->addWidget(clientCombo, 1);
  auto *newClientBtn = new QPushButton("+", clientRow);
  newClientBtn->setFixedSize(32, 32);
  newClientBtn->setStyleSheet(
      "QPushButton{background:#27272a;color:#10b981;border:none;border-radius:"
      "6px;font-weight:700;font-size:16px;}"
      "QPushButton:hover{background:#3f3f46;}");
  QObject::connect(newClientBtn, &QPushButton::clicked, [&dlg, clientCombo]() {
    int id = quickCreateClient(&dlg);
    if (id > 0) {
      Client c = ClientModel::getById(id);
      clientCombo->addItem(c.name, c.id);
      clientCombo->setCurrentIndex(clientCombo->count() - 1);
    }
  });
  clientLay->addWidget(newClientBtn);
  form->addRow("Client", clientRow);

  // Mission
  auto *missionCombo = new QComboBox(&dlg);
  missionCombo->addItem(QString::fromUtf8("\xe2\x80\x94 Aucune \xe2\x80\x94"),
                        0);
  for (const auto &m : MissionModel::all())
    missionCombo->addItem(m.title, m.id);
  for (int i = 0; i < missionCombo->count(); ++i)
    if (missionCombo->itemData(i).toInt() == quote.missionId) {
      missionCombo->setCurrentIndex(i);
      break;
    }
  form->addRow("Mission", missionCombo);

  // Dates
  auto *dateEdit = new QDateEdit(&dlg);
  dateEdit->setCalendarPopup(true);
  dateEdit->setDisplayFormat("yyyy-MM-dd");
  dateEdit->setDate(quote.date.isEmpty()
                        ? QDate::currentDate()
                        : QDate::fromString(quote.date, "yyyy-MM-dd"));
  form->addRow("Date", dateEdit);
  auto *validEdit = new QDateEdit(&dlg);
  validEdit->setCalendarPopup(true);
  validEdit->setDisplayFormat("yyyy-MM-dd");
  validEdit->setDate(quote.validUntil.isEmpty()
                         ? QDate::currentDate().addDays(30)
                         : QDate::fromString(quote.validUntil, "yyyy-MM-dd"));
  form->addRow("Valide jusqu'au", validEdit);

  // Status
  auto *statusCombo = new QComboBox(&dlg);
  statusCombo->addItems({"brouillon", "envoyee", "acceptee", "refusee"});
  int si = statusCombo->findText(quote.status);
  if (si >= 0)
    statusCombo->setCurrentIndex(si);
  form->addRow("Statut", statusCombo);

  // Tax
  auto *taxSpin = new QDoubleSpinBox(&dlg);
  taxSpin->setRange(0, 100);
  taxSpin->setSuffix(" %");
  taxSpin->setValue(quote.taxRate > 0 ? quote.taxRate : 20.0);
  form->addRow("TVA", taxSpin);

  // Notes
  auto *notesEdit = new QLineEdit(quote.notes, &dlg);
  form->addRow("Notes", notesEdit);
  layout->addLayout(form);

  // ── Items table with discount column ──
  auto *itemsLabel = new QLabel("Lignes du devis", &dlg);
  itemsLabel->setStyleSheet("color:#d97706;font-size:13px;font-weight:700;"
                            "padding:12px 0 4px 0;background:transparent;");
  layout->addWidget(itemsLabel);

  auto *itemsTable = new QTableWidget(&dlg);
  itemsTable->setColumnCount(5);
  itemsTable->setHorizontalHeaderLabels(
      {"Description", "Qte", "Prix unit.", "Remise %", "Total"});
  itemsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  for (int c = 1; c < 5; ++c)
    itemsTable->horizontalHeader()->setSectionResizeMode(
        c, QHeaderView::ResizeToContents);
  itemsTable->verticalHeader()->setVisible(false);
  itemsTable->setFixedHeight(200);
  itemsTable->setDragDropMode(QAbstractItemView::InternalMove);
  itemsTable->setDragEnabled(true);
  itemsTable->setAcceptDrops(true);
  itemsTable->setDropIndicatorShown(true);
  itemsTable->setStyleSheet("QTableWidget{background:#18181b;border:1px solid "
                            "#27272a;border-radius:6px;color:#e4e4e7;}"
                            "QHeaderView::section{background:#18181b;color:#"
                            "71717a;border:none;padding:6px;font-size:11px;}");

  // Populate existing items
  auto populateItems = [&](const QList<QuoteItem> &items) {
    itemsTable->setRowCount(items.size());
    for (int i = 0; i < items.size(); ++i) {
      itemsTable->setItem(i, 0, new QTableWidgetItem(items[i].description));
      itemsTable->setItem(
          i, 1,
          new QTableWidgetItem(QString::number(items[i].quantity, 'f', 1)));
      itemsTable->setItem(
          i, 2,
          new QTableWidgetItem(QString::number(items[i].unitPrice, 'f', 2)));
      itemsTable->setItem(
          i, 3,
          new QTableWidgetItem(QString::number(items[i].discount, 'f', 1)));
      double t = items[i].quantity * items[i].unitPrice *
                 (1.0 - items[i].discount / 100.0);
      itemsTable->setItem(i, 4,
                          new QTableWidgetItem(QString::number(t, 'f', 2)));
      if (itemsTable->item(i, 0))
        itemsTable->item(i, 0)->setData(Qt::UserRole, items[i].id);
    }
  };

  if (!isNew) {
    populateItems(QuoteModel::getItems(quote.id));
  } else if (!quote.items.isEmpty()) {
    populateItems(quote.items);
  }

  layout->addWidget(itemsTable);

  // Item buttons row + template buttons
  auto *itemBtns = new QWidget(&dlg);
  auto *itemBtnsLay = new QHBoxLayout(itemBtns);
  itemBtnsLay->setContentsMargins(0, 0, 0, 0);
  itemBtnsLay->setSpacing(4);

  auto mkBtn = [&](const QString &text, const QString &style) {
    auto *b = new QPushButton(text, itemBtns);
    b->setStyleSheet(style);
    return b;
  };
  QString grayStyle =
      "QPushButton{background:#27272a;color:#a1a1aa;border:none;border-radius:"
      "6px;padding:6px "
      "12px;font-size:12px;}QPushButton:hover{background:#3f3f46;color:white;}";
  QString redStyle =
      "QPushButton{background:#27272a;color:#ef4444;border:none;border-radius:"
      "6px;padding:6px "
      "12px;font-size:12px;}QPushButton:hover{background:#451a1a;}";
  QString purpleStyle =
      "QPushButton{background:#18181b;color:#a78bfa;border:none;border-radius:"
      "6px;padding:6px "
      "12px;font-size:12px;}QPushButton:hover{background:#27272a;}";

  auto *addItemBtn = mkBtn("+ Ligne", grayStyle);
  QObject::connect(addItemBtn, &QPushButton::clicked, [itemsTable]() {
    int r = itemsTable->rowCount();
    itemsTable->setRowCount(r + 1);
    itemsTable->setItem(r, 0, new QTableWidgetItem(""));
    itemsTable->setItem(r, 1, new QTableWidgetItem("1.0"));
    itemsTable->setItem(r, 2, new QTableWidgetItem("0.00"));
    itemsTable->setItem(r, 3, new QTableWidgetItem("0.0"));
    itemsTable->setItem(r, 4, new QTableWidgetItem("0.00"));
  });
  itemBtnsLay->addWidget(addItemBtn);

  auto *rmItemBtn = mkBtn("Supprimer ligne", redStyle);
  QObject::connect(rmItemBtn, &QPushButton::clicked, [itemsTable]() {
    int r = itemsTable->currentRow();
    if (r >= 0)
      itemsTable->removeRow(r);
  });
  itemBtnsLay->addWidget(rmItemBtn);

  itemBtnsLay->addStretch();

  // Template: save
  auto *saveTplBtn = mkBtn("Sauver modele", purpleStyle);
  QObject::connect(saveTplBtn, &QPushButton::clicked, [&dlg, itemsTable]() {
    bool ok;
    QString name = QInputDialog::getText(
        &dlg, "Sauver modele", "Nom du modele:", QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty())
      return;
    QuoteTemplate tpl;
    tpl.name = name.trimmed();
    for (int i = 0; i < itemsTable->rowCount(); ++i) {
      QuoteTemplateItem ti;
      ti.description =
          itemsTable->item(i, 0) ? itemsTable->item(i, 0)->text() : "";
      ti.quantity = itemsTable->item(i, 1)
                        ? itemsTable->item(i, 1)->text().toDouble()
                        : 1;
      ti.unitPrice = itemsTable->item(i, 2)
                         ? itemsTable->item(i, 2)->text().toDouble()
                         : 0;
      ti.discount = itemsTable->item(i, 3)
                        ? itemsTable->item(i, 3)->text().toDouble()
                        : 0;
      if (!ti.description.isEmpty())
        tpl.items.append(ti);
    }
    QuoteModel::createTemplate(tpl);
    ToastWidget::show(dlg.window(), "Modele sauvegarde: " + name,
                      ToastWidget::Success);
  });
  itemBtnsLay->addWidget(saveTplBtn);

  // Template: load
  auto *loadTplBtn = mkBtn("Charger modele", purpleStyle);
  QObject::connect(loadTplBtn, &QPushButton::clicked, [&dlg, itemsTable]() {
    auto templates = QuoteModel::allTemplates();
    if (templates.isEmpty()) {
      ToastWidget::show(dlg.window(), "Aucun modele", ToastWidget::Info);
      return;
    }
    QStringList names;
    for (const auto &t : templates)
      names << t.name;
    bool ok;
    QString sel = QInputDialog::getItem(&dlg, "Charger modele",
                                        "Modele:", names, 0, false, &ok);
    if (!ok)
      return;
    int idx = names.indexOf(sel);
    if (idx < 0)
      return;
    auto tpl = QuoteModel::getTemplate(templates[idx].id);
    itemsTable->setRowCount(tpl.items.size());
    for (int i = 0; i < tpl.items.size(); ++i) {
      itemsTable->setItem(i, 0, new QTableWidgetItem(tpl.items[i].description));
      itemsTable->setItem(
          i, 1,
          new QTableWidgetItem(QString::number(tpl.items[i].quantity, 'f', 1)));
      itemsTable->setItem(i, 2,
                          new QTableWidgetItem(
                              QString::number(tpl.items[i].unitPrice, 'f', 2)));
      itemsTable->setItem(
          i, 3,
          new QTableWidgetItem(QString::number(tpl.items[i].discount, 'f', 1)));
      double t = tpl.items[i].quantity * tpl.items[i].unitPrice *
                 (1.0 - tpl.items[i].discount / 100.0);
      itemsTable->setItem(i, 4,
                          new QTableWidgetItem(QString::number(t, 'f', 2)));
    }
    ToastWidget::show(dlg.window(), "Modele charge: " + sel,
                      ToastWidget::Success);
  });
  itemBtnsLay->addWidget(loadTplBtn);
  layout->addWidget(itemBtns);

  // OK / Cancel
  auto *buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  buttons->button(QDialogButtonBox::Ok)
      ->setStyleSheet("QPushButton{background:#f59e0b;color:white;border:none;"
                      "border-radius:6px;padding:8px 20px;font-weight:600;}"
                      "QPushButton:hover{background:#fbbf24;}");
  buttons->button(QDialogButtonBox::Cancel)
      ->setStyleSheet("QPushButton{background:#27272a;color:#a1a1aa;border:"
                      "none;border-radius:6px;padding:8px 20px;}"
                      "QPushButton:hover{background:#3f3f46;color:white;}");
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg,
                   &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg,
                   &QDialog::reject);
  layout->addWidget(buttons);

  if (dlg.exec() != QDialog::Accepted)
    return false;

  // Gather data
  quote.clientId = clientCombo->currentData().toInt();
  quote.missionId = missionCombo->currentData().toInt();
  quote.number = numberEdit->text();
  quote.date = dateEdit->date().toString("yyyy-MM-dd");
  quote.validUntil = validEdit->date().toString("yyyy-MM-dd");
  quote.status = statusCombo->currentText();
  quote.taxRate = taxSpin->value();
  quote.notes = notesEdit->text();
  quote.items.clear();
  for (int i = 0; i < itemsTable->rowCount(); ++i) {
    QuoteItem item;
    item.description =
        itemsTable->item(i, 0) ? itemsTable->item(i, 0)->text() : "";
    item.quantity = itemsTable->item(i, 1)
                        ? itemsTable->item(i, 1)->text().toDouble()
                        : 1.0;
    item.unitPrice = itemsTable->item(i, 2)
                         ? itemsTable->item(i, 2)->text().toDouble()
                         : 0.0;
    item.discount = itemsTable->item(i, 3)
                        ? itemsTable->item(i, 3)->text().toDouble()
                        : 0.0;
    item.total = item.quantity * item.unitPrice * (1.0 - item.discount / 100.0);
    if (!item.description.isEmpty())
      quote.items.append(item);
  }
  return true;
}

// ════════════════════════════════════════════════
//  DevisPage constructor + setup
// ════════════════════════════════════════════════
DevisPage::DevisPage(QWidget *parent) : QWidget(parent) {
  setObjectName("page");
  setupUi();
  setupShortcuts();
  refresh();
}

void DevisPage::setupShortcuts() {
  auto *scNew = new QShortcut(QKeySequence("Ctrl+N"), this);
  connect(scNew, &QShortcut::activated, this, &DevisPage::onAddQuote);
  auto *scDel = new QShortcut(QKeySequence(Qt::Key_Delete), this);
  connect(scDel, &QShortcut::activated, this, &DevisPage::onDeleteQuote);
  auto *scDup = new QShortcut(QKeySequence("Ctrl+D"), this);
  connect(scDup, &QShortcut::activated, this, [this]() {
    int row = m_table->currentRow();
    if (row >= 0 && m_table->item(row, 0))
      onDuplicateQuote(m_table->item(row, 0)->data(Qt::UserRole).toInt());
  });
}

void DevisPage::setupUi() {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(32, 24, 32, 24);
  mainLayout->setSpacing(16);
  auto *header = new PageHeader("Devis", "Estimations et propositions", this);
  mainLayout->addWidget(header);

  // Top bar
  auto *topBar = new QWidget(this);
  auto *topLayout = new QHBoxLayout(topBar);
  topLayout->setContentsMargins(0, 0, 0, 0);
  m_filterStatus = new QComboBox(topBar);
  m_filterStatus->addItems(
      {"Tous", "Brouillon", "Envoyee", "Acceptee", "Refusee"});
  m_filterStatus->setFixedHeight(36);
  m_filterStatus->setFixedWidth(160);
  connect(m_filterStatus, &QComboBox::currentIndexChanged, this,
          &DevisPage::onFilterChanged);
  topLayout->addWidget(m_filterStatus);
  m_searchEdit = new QLineEdit(topBar);
  m_searchEdit->setPlaceholderText("Rechercher...");
  m_searchEdit->setFixedHeight(36);
  m_searchEdit->setMaximumWidth(300);
  connect(m_searchEdit, &QLineEdit::textChanged, this,
          &DevisPage::onFilterChanged);
  topLayout->addWidget(m_searchEdit);
  topLayout->addStretch();
  auto *addBtn = new QPushButton("+ Nouveau devis", topBar);
  addBtn->setFixedHeight(36);
  addBtn->setStyleSheet(
      "QPushButton{background:#f59e0b;color:white;border:none;border-radius:"
      "8px;padding:0 "
      "20px;font-weight:600;}QPushButton:hover{background:#fbbf24;}");
  connect(addBtn, &QPushButton::clicked, this, &DevisPage::onAddQuote);
  topLayout->addWidget(addBtn);
  auto *csvBtn = new QPushButton("CSV", topBar);
  csvBtn->setFixedHeight(36);
  csvBtn->setStyleSheet("QPushButton{background:#27272a;color:#a1a1aa;border:"
                        "none;border-radius:8px;padding:0 "
                        "16px;font-weight:500;}QPushButton:hover{background:#"
                        "3f3f46;color:#e4e4e7;}");
  connect(csvBtn, &QPushButton::clicked, this, &DevisPage::onExportCsv);
  topLayout->addWidget(csvBtn);
  auto *batchDelBtn = new QPushButton("Supprimer sel.", topBar);
  batchDelBtn->setFixedHeight(36);
  batchDelBtn->setStyleSheet(
      "QPushButton{background:rgba(239,68,68,0.1);color:#ef4444;border:1px "
      "solid rgba(239,68,68,0.2);border-radius:8px;padding:0 "
      "16px;font-weight:500;}QPushButton:hover{background:rgba(239,68,68,0.2);"
      "}");
  connect(batchDelBtn, &QPushButton::clicked, this, &DevisPage::onBatchDelete);
  topLayout->addWidget(batchDelBtn);
  mainLayout->addWidget(topBar);

  // Stats
  m_statsLabel = new QLabel(this);
  m_statsLabel->setStyleSheet("color:#71717a;font-size:12px;background:"
                              "transparent;padding:0;margin:0;");
  mainLayout->addWidget(m_statsLabel);

  // Table
  m_table = new QTableWidget(this);
  m_table->setColumnCount(7);
  m_table->setHorizontalHeaderLabels({"N\xc2\xb0", "Client", "Date",
                                      "Valide jusqu'au", "Montant TTC",
                                      "Statut", ""});
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_table->verticalHeader()->setVisible(false);
  m_table->horizontalHeader()->setStretchLastSection(false);
  m_table->horizontalHeader()->setSectionResizeMode(
      0, QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  for (int c = 2; c <= 6; ++c)
    m_table->horizontalHeader()->setSectionResizeMode(
        c, QHeaderView::ResizeToContents);
  m_table->setAlternatingRowColors(true);
  m_table->setShowGrid(false);
  m_table->setStyleSheet(
      "QTableWidget{background:#18181b;border:1px solid "
      "#27272a;border-radius:8px;color:#e4e4e7;}"
      "QTableWidget::item{padding:8px;}QTableWidget::item:selected{background:#"
      "27272a;}"
      "QHeaderView::section{background:#18181b;color:#71717a;border:none;"
      "padding:8px;font-weight:600;font-size:11px;text-transform:uppercase;"
      "letter-spacing:1px;}");
  connect(m_table, &QTableWidget::cellDoubleClicked, this,
          &DevisPage::onEditQuote);
  m_table->horizontalHeader()->setSectionsClickable(true);
  connect(m_table->horizontalHeader(), &QHeaderView::sectionClicked, this,
          &DevisPage::onSortByColumn);
  mainLayout->addWidget(m_table, 1);
}

void DevisPage::onSortByColumn(int column) {
  if (column >= 6)
    return;
  if (m_sortColumn == column)
    m_sortOrder = (m_sortOrder == Qt::AscendingOrder) ? Qt::DescendingOrder
                                                      : Qt::AscendingOrder;
  else {
    m_sortColumn = column;
    m_sortOrder = Qt::AscendingOrder;
  }
  m_table->horizontalHeader()->setSortIndicator(m_sortColumn, m_sortOrder);
  populateTable();
}

void DevisPage::checkExpiredQuotes() {
  if (m_expiredChecked)
    return;
  m_expiredChecked = true;
  auto all = QuoteModel::all();
  QDate today = QDate::currentDate();
  int expired = 0;
  for (const auto &q : all) {
    if (q.status != "acceptee" && q.status != "refusee") {
      QDate v = QDate::fromString(q.validUntil, "yyyy-MM-dd");
      if (v.isValid() && v < today)
        ++expired;
    }
  }
  if (expired > 0)
    ToastWidget::show(window(), QString("%1 devis expire(s) !").arg(expired),
                      ToastWidget::Info);
}

void DevisPage::populateTable() {
  auto allQuotes = QuoteModel::all();
  QString filter =
      m_filterStatus ? m_filterStatus->currentText().toLower() : "";
  QString search = m_searchEdit ? m_searchEdit->text().toLower() : "";
  QList<Quote> filtered;
  for (const auto &q : allQuotes) {
    if (filter != "tous" && !filter.isEmpty() && q.status != filter)
      continue;
    if (!search.isEmpty() && !q.number.toLower().contains(search) &&
        !q.clientName.toLower().contains(search))
      continue;
    filtered.append(q);
  }
  // Sort
  if (m_sortColumn >= 0 && m_sortColumn < 6) {
    std::sort(filtered.begin(), filtered.end(),
              [this](const Quote &a, const Quote &b) {
                int cmp = 0;
                switch (m_sortColumn) {
                case 0:
                  cmp = a.number.compare(b.number);
                  break;
                case 1:
                  cmp = a.clientName.compare(b.clientName, Qt::CaseInsensitive);
                  break;
                case 2:
                  cmp = a.date.compare(b.date);
                  break;
                case 3:
                  cmp = a.validUntil.compare(b.validUntil);
                  break;
                case 4:
                  cmp = (a.total < b.total) ? -1 : (a.total > b.total ? 1 : 0);
                  break;
                case 5:
                  cmp = a.status.compare(b.status);
                  break;
                }
                return m_sortOrder == Qt::AscendingOrder ? cmp < 0 : cmp > 0;
              });
  }
  // Stats
  if (m_statsLabel) {
    double totalSum = 0;
    int accepted = 0, pending = 0;
    for (const auto &q : filtered) {
      totalSum += q.total;
      if (q.status == "acceptee")
        ++accepted;
      if (q.status == "envoyee" || q.status == "brouillon")
        ++pending;
    }
    m_statsLabel->setText(
        QString("<b>%1 devis</b> &mdash; Total: %2 EUR &middot; Acceptes: %3 "
                "&middot; En attente: %4")
            .arg(filtered.size())
            .arg(totalSum, 0, 'f', 2)
            .arg(accepted)
            .arg(pending));
  }
  QDate today = QDate::currentDate();
  QDate warnDate = today.addDays(7);
  m_table->setRowCount(filtered.size());
  for (int i = 0; i < filtered.size(); ++i) {
    const auto &q = filtered[i];
    m_table->setRowHeight(i, 48);
    // N°
    auto *numItem = new QTableWidgetItem(q.number);
    numItem->setData(Qt::UserRole, q.id);
    numItem->setForeground(QColor("#fbbf24"));
    QFont nf;
    nf.setBold(true);
    numItem->setFont(nf);
    m_table->setItem(i, 0, numItem);
    // Client
    m_table->setItem(
        i, 1,
        new QTableWidgetItem(q.clientName.isEmpty()
                                 ? QString::fromUtf8("\xe2\x80\x94")
                                 : q.clientName));
    // Date
    m_table->setItem(i, 2, new QTableWidgetItem(q.date));
    // Valid — expiration coloring
    auto *vi = new QTableWidgetItem(q.validUntil);
    QDate vd = QDate::fromString(q.validUntil, "yyyy-MM-dd");
    if (vd.isValid()) {
      if (vd < today)
        vi->setForeground(QColor("#ef4444"));
      else if (vd <= warnDate)
        vi->setForeground(QColor("#f59e0b"));
    }
    m_table->setItem(i, 3, vi);
    // Amount
    auto *ai = new QTableWidgetItem(QString("%1 EUR").arg(q.total, 0, 'f', 2));
    ai->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QFont af;
    af.setBold(true);
    ai->setFont(af);
    if (q.total > 0)
      ai->setForeground(QColor("#10b981"));
    m_table->setItem(i, 4, ai);
    // Status badge
    auto *sw = new QWidget();
    auto *sl = new QHBoxLayout(sw);
    sl->setContentsMargins(4, 4, 4, 4);
    auto *slab = new QLabel(q.status);
    slab->setAlignment(Qt::AlignCenter);
    if (q.status == "acceptee")
      slab->setObjectName("statusBadge_success");
    else if (q.status == "refusee")
      slab->setObjectName("statusBadge_danger");
    else
      slab->setObjectName("statusBadge_warning");
    slab->style()->unpolish(slab);
    slab->style()->polish(slab);
    sl->addWidget(slab);
    m_table->setCellWidget(i, 5, sw);
    // Actions
    auto *aw = new QWidget();
    auto *al = new QHBoxLayout(aw);
    al->setContentsMargins(4, 0, 4, 0);
    al->setSpacing(4);
    auto mkA = [](const QString &t, int w, const QString &s) {
      auto *b = new QPushButton(t);
      b->setFixedSize(w, 28);
      b->setStyleSheet(s);
      return b;
    };
    auto *editB = mkA("Modifier", 80,
                      "QPushButton{background:#27272a;color:#a1a1aa;border:"
                      "none;border-radius:6px;font-size:11px;}QPushButton:"
                      "hover{background:#3f3f46;color:white;}");
    connect(editB, &QPushButton::clicked, this,
            [this, i]() { onEditQuote(i); });
    al->addWidget(editB);
    auto *delB = mkA(
        "\xE2\x9C\x95", 28,
        "QPushButton{background:#27272a;color:#ef4444;border:none;border-"
        "radius:6px;font-size:12px;}QPushButton:hover{background:#451a1a;}");
    connect(delB, &QPushButton::clicked, this, [this, i]() {
      int id = m_table->item(i, 0)->data(Qt::UserRole).toInt();
      if (QMessageBox::question(this, "Supprimer", "Supprimer ce devis?") ==
          QMessageBox::Yes) {
        QuoteModel::remove(id);
        ActivityLog::log("delete", "devis", "Devis #" + QString::number(id));
        ToastWidget::show(window(), "Devis supprime", ToastWidget::Success);
        refresh();
      }
    });
    al->addWidget(delB);
    auto *dupB = mkA("Dup.", 50,
                     "QPushButton{background:#18181b;color:#a78bfa;border:none;"
                     "border-radius:6px;font-size:11px;font-weight:600;}"
                     "QPushButton:hover{background:#27272a;}");
    connect(dupB, &QPushButton::clicked, this, [this, i]() {
      onDuplicateQuote(m_table->item(i, 0)->data(Qt::UserRole).toInt());
    });
    al->addWidget(dupB);
    auto *convB = mkA("Fact.", 50,
                      "QPushButton{background:#b45309;color:white;border:none;"
                      "border-radius:6px;font-size:11px;font-weight:600;}"
                      "QPushButton:hover{background:#d97706;}");
    connect(convB, &QPushButton::clicked, this, [this, i]() {
      onConvertToInvoice(m_table->item(i, 0)->data(Qt::UserRole).toInt());
    });
    al->addWidget(convB);
    auto *pdfB = mkA("PDF", 50,
                     "QPushButton{background:#18181b;color:#fbbf24;border:none;"
                     "border-radius:6px;font-size:11px;font-weight:600;}"
                     "QPushButton:hover{background:#27272a;}");
    connect(pdfB, &QPushButton::clicked, this, [this, i]() {
      exportPdf(m_table->item(i, 0)->data(Qt::UserRole).toInt());
    });
    al->addWidget(pdfB);
    m_table->setCellWidget(i, 6, aw);
  }
}

void DevisPage::refresh() {
  populateTable();
  checkExpiredQuotes();
}
void DevisPage::onFilterChanged() { populateTable(); }

void DevisPage::onAddQuote() {
  Quote quote;
  if (!showQuoteDialog(this, quote, true))
    return;
  int id = QuoteModel::create(quote);
  if (id > 0) {
    for (auto &item : quote.items) {
      item.quoteId = id;
      QuoteModel::addItem(item);
    }
    QuoteModel::recalculate(id);
    ActivityLog::log("create", "devis", "Devis: " + quote.number);
    ToastWidget::show(window(), "Devis cree: " + quote.number,
                      ToastWidget::Success);
  }
  refresh();
}

void DevisPage::onEditQuote(int row) {
  if (row < 0 || !m_table->item(row, 0))
    return;
  int id = m_table->item(row, 0)->data(Qt::UserRole).toInt();
  Quote quote = QuoteModel::getById(id);
  if (quote.id == 0)
    return;
  if (!showQuoteDialog(this, quote, false))
    return;
  QuoteModel::update(quote);
  auto oldItems = QuoteModel::getItems(quote.id);
  for (const auto &old : oldItems)
    QuoteModel::removeItem(old.id);
  for (auto &item : quote.items) {
    item.quoteId = quote.id;
    QuoteModel::addItem(item);
  }
  QuoteModel::recalculate(quote.id);
  ActivityLog::log("edit", "devis", "Devis: " + quote.number);
  ToastWidget::show(window(), "Devis modifie: " + quote.number,
                    ToastWidget::Success);
  refresh();
}

void DevisPage::onDeleteQuote() {
  int row = m_table->currentRow();
  if (row < 0 || !m_table->item(row, 0))
    return;
  int id = m_table->item(row, 0)->data(Qt::UserRole).toInt();
  if (QMessageBox::question(this, "Supprimer", "Supprimer ce devis?") ==
      QMessageBox::Yes) {
    QuoteModel::remove(id);
    ActivityLog::log("delete", "devis", "Devis #" + QString::number(id));
    ToastWidget::show(window(), "Devis supprime", ToastWidget::Success);
    refresh();
  }
}

void DevisPage::onDuplicateQuote(int quoteId) {
  Quote original = QuoteModel::getById(quoteId);
  if (original.id == 0)
    return;
  Quote dup;
  dup.clientId = original.clientId;
  dup.missionId = original.missionId;
  dup.date = QDate::currentDate().toString("yyyy-MM-dd");
  dup.validUntil = QDate::currentDate().addDays(30).toString("yyyy-MM-dd");
  dup.status = "brouillon";
  dup.taxRate = original.taxRate;
  dup.notes = original.notes;
  dup.items = original.items;
  if (!showQuoteDialog(this, dup, true))
    return;
  int id = QuoteModel::create(dup);
  if (id > 0) {
    for (auto &item : dup.items) {
      item.quoteId = id;
      item.id = 0;
      QuoteModel::addItem(item);
    }
    QuoteModel::recalculate(id);
    ActivityLog::log("create", "devis",
                     "Duplique: " + dup.number + " (depuis " + original.number +
                         ")");
    ToastWidget::show(window(), "Devis duplique: " + dup.number,
                      ToastWidget::Success);
  }
  refresh();
}

void DevisPage::onConvertToInvoice(int quoteId) {
  if (QMessageBox::question(this, "Convertir",
                            "Convertir ce devis en facture?") !=
      QMessageBox::Yes)
    return;
  int invoiceId = QuoteModel::convertToInvoice(quoteId);
  if (invoiceId > 0) {
    ActivityLog::log("create", "facture",
                     "Devis #" + QString::number(quoteId) + " -> Facture #" +
                         QString::number(invoiceId));
    ToastWidget::show(window(),
                      QString("Converti en facture #%1").arg(invoiceId),
                      ToastWidget::Success);
    refresh();
  } else {
    ToastWidget::show(window(), "Echec de la conversion", ToastWidget::Error);
  }
}

// ════════════════════════════════════════════════
//  PDF Export (with CGV from settings)
// ════════════════════════════════════════════════
static QString devisGetSetting(const QString &key, const QString &def = "") {
  QSqlQuery q(Database::instance().db());
  q.prepare("SELECT value FROM settings WHERE key = ?");
  q.addBindValue(key);
  q.exec();
  if (q.next())
    return q.value(0).toString();
  return def;
}

void DevisPage::exportPdf(int quoteId) {
  Quote quote = QuoteModel::getById(quoteId);
  if (quote.id == 0)
    return;
  QString defaultPath =
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
      "/Devis_" + quote.number + ".pdf";
  QString filePath = QFileDialog::getSaveFileName(this, "Exporter PDF",
                                                  defaultPath, "PDF (*.pdf)");
  if (filePath.isEmpty())
    return;
  QString companyName = devisGetSetting("company_name", "BlackLys Studio");
  QString companyAddr = devisGetSetting("company_address");
  QString companyPhone = devisGetSetting("company_phone");
  QString companyEmail = devisGetSetting("company_email");
  QString companySiret = devisGetSetting("company_siret");
  QString cgv = devisGetSetting("quote_cgv");

  QPdfWriter writer(filePath);
  writer.setPageSize(QPageSize(QPageSize::A4));
  writer.setPageMargins(QMarginsF(20, 20, 20, 20), QPageLayout::Millimeter);
  writer.setResolution(300);
  QPainter painter(&writer);
  if (!painter.isActive()) {
    ToastWidget::show(window(), "Erreur PDF", ToastWidget::Error);
    return;
  }

  int pageW = writer.width(), pageH = writer.height(), y = 0;
  QFont titleFont("Segoe UI", 18, QFont::Bold),
      headerFont("Segoe UI", 10, QFont::Bold);
  QFont normalFont("Segoe UI", 9), smallFont("Segoe UI", 7);

  painter.setFont(titleFont);
  painter.setPen(QColor("#18181b"));
  painter.drawText(0, y, pageW / 2, 300, Qt::AlignLeft | Qt::AlignTop,
                   companyName);
  painter.drawText(pageW / 2, y, pageW / 2, 300, Qt::AlignRight | Qt::AlignTop,
                   "DEVIS " + quote.number);
  y += 350;
  painter.setFont(smallFont);
  painter.setPen(QColor("#71717a"));
  for (const auto &line :
       QStringList{companyAddr, companyPhone, companyEmail,
                   companySiret.isEmpty() ? "" : "SIRET: " + companySiret}) {
    if (!line.isEmpty()) {
      painter.drawText(0, y, pageW, 120, Qt::AlignLeft, line);
      y += 130;
    }
  }
  y += 100;
  painter.setFont(headerFont);
  painter.setPen(QColor("#18181b"));
  painter.drawText(0, y, pageW / 2, 200, Qt::AlignLeft, "CLIENT");
  painter.drawText(pageW / 2, y, pageW / 2, 200, Qt::AlignRight,
                   "Date: " + quote.date);
  y += 200;
  painter.setFont(normalFont);
  painter.setPen(QColor("#3f3f46"));
  painter.drawText(0, y, pageW / 2, 160, Qt::AlignLeft, quote.clientName);
  painter.drawText(pageW / 2, y, pageW / 2, 160, Qt::AlignRight,
                   "Valide: " + quote.validUntil);
  y += 300;

  // Items header
  int colD = pageW * 40 / 100, colQ = pageW * 10 / 100, colU = pageW * 16 / 100,
      colR = pageW * 14 / 100, colT = pageW * 20 / 100, rowH = 180;
  painter.fillRect(0, y, pageW, rowH, QColor("#f4f4f5"));
  painter.setFont(headerFont);
  painter.setPen(QColor("#18181b"));
  int x = 40;
  painter.drawText(x, y, colD, rowH, Qt::AlignLeft | Qt::AlignVCenter,
                   "Description");
  x += colD;
  painter.drawText(x, y, colQ, rowH, Qt::AlignCenter, "Qte");
  x += colQ;
  painter.drawText(x, y, colU, rowH, Qt::AlignRight | Qt::AlignVCenter,
                   "Prix unit.");
  x += colU;
  painter.drawText(x, y, colR, rowH, Qt::AlignRight | Qt::AlignVCenter,
                   "Remise");
  x += colR;
  painter.drawText(x, y, colT - 40, rowH, Qt::AlignRight | Qt::AlignVCenter,
                   "Total");
  y += rowH;

  // Items
  painter.setFont(normalFont);
  double subtotal = 0;
  for (const auto &item : quote.items) {
    painter.setPen(QColor("#e4e4e7"));
    painter.drawLine(0, y, pageW, y);
    painter.setPen(QColor("#3f3f46"));
    x = 40;
    painter.drawText(x, y, colD, rowH, Qt::AlignLeft | Qt::AlignVCenter,
                     item.description);
    x += colD;
    painter.drawText(x, y, colQ, rowH, Qt::AlignCenter,
                     QString::number(item.quantity, 'f', 1));
    x += colQ;
    painter.drawText(x, y, colU, rowH, Qt::AlignRight | Qt::AlignVCenter,
                     QString("%1 EUR").arg(item.unitPrice, 0, 'f', 2));
    x += colU;
    painter.drawText(x, y, colR, rowH, Qt::AlignRight | Qt::AlignVCenter,
                     item.discount > 0
                         ? QString("-%1%").arg(item.discount, 0, 'f', 1)
                         : "-");
    x += colR;
    painter.drawText(x, y, colT - 40, rowH, Qt::AlignRight | Qt::AlignVCenter,
                     QString("%1 EUR").arg(item.total, 0, 'f', 2));
    subtotal += item.total;
    y += rowH;
  }
  painter.setPen(QColor("#18181b"));
  painter.drawLine(0, y, pageW, y);
  y += 100;

  // Totals
  double tva = subtotal * quote.taxRate / 100.0, ttc = subtotal + tva;
  int tX = pageW * 60 / 100, tW = pageW - tX;
  painter.setFont(normalFont);
  painter.setPen(QColor("#3f3f46"));
  painter.drawText(tX, y, tW / 2, rowH, Qt::AlignLeft, "Sous-total HT");
  painter.drawText(tX + tW / 2, y, tW / 2, rowH, Qt::AlignRight,
                   QString("%1 EUR").arg(subtotal, 0, 'f', 2));
  y += rowH;
  painter.drawText(tX, y, tW / 2, rowH, Qt::AlignLeft,
                   QString("TVA %1%").arg(quote.taxRate, 0, 'f', 1));
  painter.drawText(tX + tW / 2, y, tW / 2, rowH, Qt::AlignRight,
                   QString("%1 EUR").arg(tva, 0, 'f', 2));
  y += rowH;
  painter.setFont(headerFont);
  painter.setPen(QColor("#18181b"));
  painter.fillRect(tX - 20, y - 10, tW + 40, rowH + 20, QColor("#f4f4f5"));
  painter.drawText(tX, y, tW / 2, rowH, Qt::AlignLeft, "Total TTC");
  painter.drawText(tX + tW / 2, y, tW / 2, rowH, Qt::AlignRight,
                   QString("%1 EUR").arg(ttc, 0, 'f', 2));
  y += rowH + 200;

  // Notes
  if (!quote.notes.isEmpty()) {
    painter.setFont(smallFont);
    painter.setPen(QColor("#71717a"));
    painter.drawText(0, y, pageW, 300, Qt::AlignLeft | Qt::TextWordWrap,
                     "Notes: " + quote.notes);
    y += 350;
  }
  // CGV
  if (!cgv.isEmpty()) {
    painter.setFont(smallFont);
    painter.setPen(QColor("#a1a1aa"));
    QRect cgvRect(0, y, pageW, pageH - y - 200);
    painter.drawText(cgvRect, Qt::AlignLeft | Qt::TextWordWrap, cgv);
  }
  // Footer
  painter.setFont(smallFont);
  painter.setPen(QColor("#a1a1aa"));
  painter.drawText(0, pageH - 200, pageW, 200, Qt::AlignCenter,
                   QString("Valable jusqu'au %1 \xe2\x80\x94 %2")
                       .arg(quote.validUntil, companyName));
  painter.end();
  QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
  ActivityLog::log("export", "devis", "PDF: " + quote.number);
  ToastWidget::show(window(), "PDF exporte: " + quote.number,
                    ToastWidget::Success);
}

void DevisPage::onBatchDelete() {
  auto sel = m_table->selectionModel()->selectedRows();
  if (sel.isEmpty())
    return;
  if (QMessageBox::warning(
          this, "Supprimer", QString("Supprimer %1 devis?").arg(sel.size()),
          QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
    return;
  for (const auto &idx : sel) {
    int id = m_table->item(idx.row(), 0)->data(Qt::UserRole).toInt();
    QuoteModel::remove(id);
    ActivityLog::log("delete", "devis", "Devis #" + QString::number(id));
  }
  ToastWidget::show(window(), QString("%1 devis supprime(s)").arg(sel.size()),
                    ToastWidget::Success);
  refresh();
}

void DevisPage::onExportCsv() {
  QString path = QFileDialog::getSaveFileName(this, "Exporter", "devis.csv",
                                              "CSV (*.csv)");
  if (path.isEmpty())
    return;
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    return;
  QTextStream out(&file);
  out << "Numero;Client;Date;Valide;Montant TTC;Statut\n";
  for (const auto &q : QuoteModel::all())
    out << q.number << ";" << q.clientName << ";" << q.date << ";"
        << q.validUntil << ";" << QString::number(q.total, 'f', 2) << ";"
        << q.status << "\n";
  file.close();
  ToastWidget::show(window(), "CSV exporte", ToastWidget::Success);
}
