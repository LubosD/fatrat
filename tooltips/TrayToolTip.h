#ifndef TRAYTOOLTIP_H
#define TRAYTOOLTIP_H
#include "BaseToolTip.h"
#include <QObject>
#include <QVector>
#include <QPair>

class TrayToolTip : public BaseToolTip
{
Q_OBJECT
public:
	TrayToolTip(QWidget* parent = 0);
	void regMove();
	void drawGraph(QPainter* painter);
	void redraw();
	void updateData();
	virtual void refresh();
private:
	QVector<int> m_speeds[2];
	QWidget* m_object;
};

#endif
