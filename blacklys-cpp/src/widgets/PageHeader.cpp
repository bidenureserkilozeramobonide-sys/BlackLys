#include "PageHeader.h"
#include <QFont>
#include <QHBoxLayout>
#include <QVBoxLayout>


PageHeader::PageHeader(const QString &title, const QString &subtitle,
                       QWidget *parent)
    : QWidget(parent) {
  setObjectName("pageHeader");
  setFixedHeight(80);
  setupUi(title, subtitle);
}

void PageHeader::setupUi(const QString &title, const QString &subtitle) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(32, 16, 32, 16);
  layout->setSpacing(4);

  m_title = new QLabel(title, this);
  m_title->setObjectName("pageTitle");
  QFont titleFont("Segoe UI", 20);
  titleFont.setWeight(QFont::DemiBold);
  m_title->setFont(titleFont);
  layout->addWidget(m_title);

  if (!subtitle.isEmpty()) {
    m_subtitle = new QLabel(subtitle, this);
    m_subtitle->setObjectName("pageSubtitle");
    layout->addWidget(m_subtitle);
  }

  layout->addStretch();
}

void PageHeader::setTitle(const QString &title) {
  if (m_title)
    m_title->setText(title);
}

void PageHeader::setSubtitle(const QString &subtitle) {
  if (!m_subtitle) {
    m_subtitle = new QLabel(subtitle, this);
    m_subtitle->setObjectName("pageSubtitle");
    if (auto *lay = qobject_cast<QVBoxLayout *>(layout())) {
      lay->insertWidget(1, m_subtitle);
    }
  } else {
    m_subtitle->setText(subtitle);
  }
}
