/*
 * Based on Qt source code
 */

#ifndef BALOONTIP_H
#define BALOONTIP_H
#include <QIcon>
#include <QPixmap>
#include <QPoint>
#include <QTimer>
#include <QWidget>

class BalloonTip : public QWidget {
  Q_OBJECT
 public:
  BalloonTip(QWidget* parent, QIcon icon, const QString& title,
             const QString& msg);
  ~BalloonTip();
  void balloon(const QPoint& pos, int msecs, bool showArrow = true);
 signals:
  void messageClicked();
  void manuallyClosed();
 private slots:
  void timerEvent();
  void closeButtonClicked();

 protected:
  void paintEvent(QPaintEvent*);
  void resizeEvent(QResizeEvent*);
  void mousePressEvent(QMouseEvent* e);

 private:
  QTimer m_timer;
  QPixmap pixmap;
};

#endif  // BALOONTIP_H
