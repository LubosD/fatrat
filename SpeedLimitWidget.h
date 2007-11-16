#ifndef SPEEDLIMITWIDGET_H
#define SPEEDLIMITWIDGET_H
#include <QWidget>
#include <QLabel>
#include <QTimer>

class RightClickLabel : public QLabel
{
Q_OBJECT
public:
	RightClickLabel(QWidget* parent) : QLabel(parent), m_nSpeed(0), m_bUpload(false)
	{
	}
	void setUpload(bool is) { m_bUpload = is; }
	void refresh(int speed) { m_nSpeed = speed; }
public slots:
	void setLimit();
protected:
	void mousePressEvent(QMouseEvent* event);
	
	int m_nSpeed;
	bool m_bUpload;
};

#include "ui_SpeedLimitWidget.h"

class SpeedLimitWidget : public QWidget, Ui_SpeedLimitWidget
{
Q_OBJECT
public:
	SpeedLimitWidget(QWidget* parent);
public slots:
	void refresh();
protected:
	void mouseDoubleClickEvent(QMouseEvent*);
private:
	QTimer m_timer;
};

#endif
