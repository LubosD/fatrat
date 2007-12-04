#ifndef _SPEEDGRAPH_H
#define _SPEEDGRAPH_H
#include <QWidget>
#include <QPainter>
#include <QPaintEvent>
#include <QTimer>
#include "Transfer.h"

class SpeedGraph : public QWidget
{
Q_OBJECT
public:
	SpeedGraph(QWidget* parent);
	void setRenderSource(Transfer* t);
public slots:
	void setNull() { setRenderSource(0); }
	void saveScreenshot();
protected:
	virtual void paintEvent(QPaintEvent* event);
	virtual void contextMenuEvent(QContextMenuEvent* event);
	void draw(QPaintDevice* device, QPaintEvent* event = 0);
	void drawNoData(QPainter& painter);
	
	Transfer* m_transfer;
	QTimer* m_timer;
};

#endif
