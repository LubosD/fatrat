#include "TrayToolTip.h"
#include "QueueMgr.h"
#include "fatrat.h"
#include <QPicture>
#include <QPainter>
#include <QApplication>
#include <QtDebug>

const int VALUES = 40;
const int WIDTH = 250;
const int HEIGHT = 80;
const int OFFSET = 20;

//const int SECSTOHIDE = 3;

TrayToolTip::TrayToolTip(QWidget* parent)
	: BaseToolTip(0, parent)
{
	m_speeds[0].fill(0, VALUES);
	m_speeds[1].fill(0, VALUES);
	
	resize(WIDTH, HEIGHT);
	setAlignment(Qt::AlignTop);
	placeMe();
	redraw();
}

void TrayToolTip::regMove()
{
	if(!isVisible())
	{
		placeMe();
		m_object = QApplication::widgetAt(QCursor::pos());
		show();
	}
}

void TrayToolTip::refresh()
{
	if(QApplication::widgetAt(QCursor::pos()) != m_object)
		hide();
	else
		redraw();
}

void TrayToolTip::redraw()
{
	if(m_speeds[0].size() >= VALUES)
	{
		m_speeds[0].pop_front();
		m_speeds[1].pop_front();
	}
	
	int downt = QueueMgr::instance()->totalDown();
	int upt = QueueMgr::instance()->totalUp();
	
	m_speeds[0].push_back(downt);
	m_speeds[1].push_back(upt);
	
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
	
	for(int i=0;i<m_speeds[0].size();i++)
		top = std::max(top, std::max(m_speeds[0][i], m_speeds[1][i]));
	
	for(int j=0;j<2;j++)
	{
		painter->setPen((!j) ? Qt::blue : Qt::red);
		
		for(int i=m_speeds[j].size()-1;i>0;i--)
		{
			int xfrom, xto, yfrom, yto;
			xfrom = float(WIDTH)/VALUES*(i);
			xto = float(WIDTH)/VALUES*(i-1);
			yfrom = height - float(height)/top*m_speeds[j][i];
			yto = height - float(height)/top*m_speeds[j][i-1];
			
			yfrom += OFFSET;
			yto += OFFSET;
			
			painter->drawLine(xfrom, yfrom, xto, yto);
		}
	}
}

