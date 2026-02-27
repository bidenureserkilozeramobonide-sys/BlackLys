#pragma once

#include <QWidget>
#include <QLabel>

class PageHeader : public QWidget
{
    Q_OBJECT

public:
    explicit PageHeader(const QString &title, const QString &subtitle = "",
                        QWidget *parent = nullptr);

    void setTitle(const QString &title);
    void setSubtitle(const QString &subtitle);

private:
    void setupUi(const QString &title, const QString &subtitle);

    QLabel *m_title = nullptr;
    QLabel *m_subtitle = nullptr;
};
