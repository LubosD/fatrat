#include "StatsWidget.h"
#include "NetIface.h"
#include "QueueMgr.h"
#include <QPainter>
#include <QPaintEvent>
#include <QSettings>
#include <QtDebug>

extern QSettings* g_settings;

StatsWidget::StatsWidget(QWidget* parent) : QWidget(parent)
{
	m_globDown = m_globUp = -1;
	m_globDownPrev = m_globUpPrev = -1;
	
	refresh();
}

void StatsWidget::refresh()
{
	QString iface = getRoutingInterface4();
	
	if(!iface.isEmpty())
	{
		QPair<qint64, qint64> newv = getInterfaceStats(iface);
		
		if(newv.first >= 0)
		{
			m_globDownPrev = m_globDown;
			m_globUpPrev = m_globUp;
			m_globDown = newv.first;
			m_globUp = newv.second;
		}
	}
	update();
}

void StatsWidget::paintEvent(QPaintEvent* event)
{
	QPainter painter(this);
	
	painter.setRenderHint(QPainter::Antialiasing);
	
	if(event != 0)
	{
		painter.setClipRegion(event->region());
		painter.fillRect(event->rect(), QBrush(Qt::white));
	}
	else
		painter.fillRect(QRect(QPoint(0, 0), size()), QBrush(Qt::white));
	
	if(m_globDownPrev >= 0)
	{
		int tmp;
		const int SPACE = 15;
		const int colwidth = width() / 8;
		const int colheight = height()-SPACE*2;
		qint64 mydown = QueueMgr::instance()->totalDown();
		qint64 myup = QueueMgr::instance()->totalUp();
		qint64 globdown = m_globDown - m_globDownPrev;
		qint64 globup = m_globUp - m_globUpPrev;
		const int maxdown = g_settings->value("network/speed_down", 1024).toInt();
		const int maxup = g_settings->value("network/speed_up", 1024).toInt();
		
		globdown = qMin<qint64>(globdown, maxdown);
		globup = qMin<qint64>(globup, maxup);
		
		mydown = qMin<qint64>(mydown, globdown);
		myup = qMin<qint64>(myup, globup);
		
		painter.drawRect(colwidth, SPACE, colwidth, colheight);
		
		tmp = double(colheight)/maxdown*globdown;
		painter.fillRect(colwidth, colheight+SPACE, colwidth, -tmp, Qt::black);
		tmp = double(colheight)/maxdown*mydown;
		painter.fillRect(colwidth, colheight+SPACE, colwidth, -tmp, Qt::red);
		
		painter.drawRect(width()-colwidth, SPACE, -colwidth, colheight);
		
		tmp = double(colheight)/maxup*globup;
		painter.fillRect(width()-colwidth, colheight+SPACE, -colwidth, -tmp, Qt::black);
		tmp = double(colheight)/maxup*myup;
		painter.fillRect(width()-colwidth, colheight+SPACE, -colwidth, -tmp, Qt::red);
		
		painter.drawText(QRect(0, 0, colwidth*3, SPACE), Qt::AlignCenter, "down");
		painter.drawText(QRect(width()-colwidth*3, 0, colwidth*3, SPACE), Qt::AlignCenter, "up");
	}
}
