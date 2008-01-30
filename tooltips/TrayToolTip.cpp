#include "TrayToolTip.h"
#include "Queue.h"
#include "fatrat.h"
#include <QPicture>
#include <QPainter>
#include <QtDebug>

extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;

const int VALUES = 40;
const int WIDTH = 250;
const int HEIGHT = 80;
const int OFFSET = 20;

const int SECSTOHIDE = 3;

TrayToolTip::TrayToolTip(QWidget* parent)
	: BaseToolTip(0, parent), m_ticks(0)
{
	m_values.fill(QPair<int,int>(0, 0), VALUES);
	
	resize(WIDTH, HEIGHT);
	setAlignment(Qt::AlignTop);
	placeMe();
	refresh();
}

void TrayToolTip::regMove()
{
	placeMe();
	show();
	m_ticks = 0;
}

void TrayToolTip::refresh()
{
	if(++m_ticks >= SECSTOHIDE)
	{
		hide();
		return;
	}
	
	g_queuesLock.lockForRead();
	
	int upt = 0, downt = 0;
	
	for(int j=0;j<g_queues.size();j++)
	{
		Queue* q = g_queues[j];
		q->lock();
		
		for(int i=0;i<q->size();i++)
		{
			int up,down;
			q->at(i)->speeds(down,up);
			downt += down;
			upt += up;
		}
		
		q->unlock();
	}
	
	g_queuesLock.unlock();
	
	if(m_values.size() >= VALUES)
		m_values.pop_front();
	m_values.push_back( QPair<int,int>(downt, upt) );
	
	QPixmap pixmap(WIDTH, HEIGHT);
	QPainter painter;
	
	pixmap.fill(Qt::white);
	painter.begin(&pixmap);
	
	QString text = QString("%1 down | %2 up").arg(formatSize(downt,true)).arg(formatSize(upt,true));
	painter.setPen(Qt::black);
	painter.drawText(0, 0, WIDTH, OFFSET, Qt::AlignTop|Qt::AlignHCenter, text, 0);
	
	painter.setPen(Qt::gray);
	painter.setFont( QFont(QString(), 25) );
	painter.drawText(0, 0, WIDTH, HEIGHT, Qt::AlignCenter, "FatRat", 0);
	
	drawGraph(&painter);
	
	painter.end();
	setPixmap(pixmap);
}

void TrayToolTip::drawGraph(QPainter* painter)
{
	int top = 0;
	const int height = HEIGHT-OFFSET - 5;
	
	for(int i=0;i<m_values.size();i++)
		top = std::max(top, std::max(m_values[i].first,m_values[i].second));
	
	painter->setPen(Qt::blue);
	for(int i=m_values.size()-1;i>0;i--)
	{
		int xfrom, xto, yfrom, yto;
		xfrom = float(WIDTH)/VALUES*(i);
		xto = float(WIDTH)/VALUES*(i-1);
		yfrom = height - float(height)/top*m_values[i].first;
		yto = height - float(height)/top*m_values[i-1].first;
		
		yfrom += OFFSET;
		yto += OFFSET;
		
		painter->drawLine(xfrom, yfrom, xto, yto);
	}
	
	// FIXME: long live the copy&paste...
	painter->setPen(Qt::red);
	for(int i=m_values.size()-1;i>0;i--)
	{
		int xfrom, xto, yfrom, yto;
		xfrom = float(WIDTH)/VALUES*(i);
		xto = float(WIDTH)/VALUES*(i-1);
		yfrom = height - float(height)/top*m_values[i].second;
		yto = height - float(height)/top*m_values[i-1].second;
		
		yfrom += OFFSET;
		yto += OFFSET;
		
		painter->drawLine(xfrom, yfrom, xto, yto);
	}
}

