#ifndef _SPEEDGRAPH_H
#define _SPEEDGRAPH_H
#include <QWidget>
#include <QPainter>
#include <QPaintEvent>
#include <QTimer>
#include <QSettings>
#include "Transfer.h"

class SpeedGraph : public QWidget
{
Q_OBJECT
public:
	SpeedGraph(QWidget* parent);
	void setRenderSource(Transfer* t);
public slots:
	void setNull() { setRenderSource(0); }
protected:
	virtual void paintEvent(QPaintEvent* event);
	void drawNoData(QPainter& painter);
	
	Transfer* m_transfer;
	QTimer* m_timer;
	QSettings m_settings;
};

#endif
