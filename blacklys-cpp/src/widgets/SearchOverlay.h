#pragma once

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QWidget>

class SearchOverlay : public QWidget {
  Q_OBJECT
  Q_PROPERTY(
      qreal backdropOpacity READ backdropOpacity WRITE setBackdropOpacity)

public:
  explicit SearchOverlay(QWidget *parent = nullptr);
  void toggle();
  qreal backdropOpacity() const { return m_backdropAlpha; }
  void setBackdropOpacity(qreal v) {
    m_backdropAlpha = v;
    update();
  }

signals:
  void resultSelected(int pageIndex, int itemId);

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;

private slots:
  void onTextChanged(const QString &text);
  void onItemClicked(QListWidgetItem *item);

private:
  QWidget *m_panel = nullptr;
  QLineEdit *m_input = nullptr;
  QListWidget *m_results = nullptr;
  QLabel *m_hint = nullptr;
  qreal m_backdropAlpha = 0;
  bool m_animating = false;
};
