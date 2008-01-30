#ifndef BASETOOLTIP_H
#define BASETOOLTIP_H
#include <QLabel>
#include <QTimer>

class BaseToolTip : public QLabel
{
Q_OBJECT
public:
	BaseToolTip(QObject* master = 0, QWidget* parent = 0);
	void placeMe();
	virtual void refresh() = 0;
public slots:
	void doRefresh() { refresh(); } // is this necessary?
protected:
	virtual void mousePressEvent(QMouseEvent*);
private:
	QTimer m_timer;
};

#endif
