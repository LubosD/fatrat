/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
USA.
*/

#include "TrayToolTip.h"
#include "QueueMgr.h"
#include "fatrat.h"
#include <QPicture>
#include <QPainter>
#include <QApplication>
#include <QtDebug>

const int VALUES = 60;
const int WIDTH = 250;
const int HEIGHT = 80;
const int OFFSET = 20;

TrayToolTip::TrayToolTip(QWidget* parent)
	: BaseToolTip(0, parent)
{
	m_speeds[0].fill(0, VALUES);
	m_speeds[1].fill(0, VALUES);
	
	resize(WIDTH, HEIGHT);
	setAlignment(Qt::AlignTop);
	placeMe();
	
	updateData();
}

void TrayToolTip::regMove()
{
	if(!isVisible())
	{
		placeMe();
		m_object = QApplication::widgetAt(QCursor::pos());
		redraw();
		show();
	}
}

void TrayToolTip::refresh()
{
	updateData();
	
	if(QApplication::widgetAt(QCursor::pos()) != m_object)
		hide();
	else
		redraw();
}

void TrayToolTip::updateData()
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
}

void TrayToolTip::redraw()
{
	QPixmap pixmap(WIDTH, HEIGHT);
	QPainter painter(&pixmap);
	
	int downt = QueueMgr::instance()->totalDown();
	int upt = QueueMgr::instance()->totalUp();
	
	pixmap.fill(Qt::white);
	
	QString text = QString("%1 down | %2 up").arg(formatSize(downt,true)).arg(formatSize(upt,true));
	painter.setPen(Qt::black);
	painter.drawText(0, 0, WIDTH, OFFSET, Qt::AlignTop|Qt::AlignHCenter, text, 0);
	
	painter.setPen(Qt::gray);
	painter.setFont( QFont(QString(), 25) );
	painter.drawText(0, 0, WIDTH, HEIGHT, Qt::AlignCenter, "FatRat", 0);
	
	drawGraph(&painter);
	
	setPixmap(pixmap);
}

void TrayToolTip::drawGraph(QPainter* painter)
{
	int top = 10;
	const int height = HEIGHT-OFFSET - 5;
	const int arsize = (VALUES-1) * 2;
	float perbyte;
	QPoint points[arsize];
	
	for(int i=0;i<m_speeds[0].size();i++)
		top = std::max(top, std::max(m_speeds[0][i], m_speeds[1][i]));
	
	perbyte = float(height)/top;
	
	for(int j=0;j<2;j++)
	{
		painter->setPen((!j) ? Qt::blue : Qt::red);
		
		int x = 0;
		
		for(int i=0; i<m_speeds[j].size(); i++)
		{
			int xfrom, yfrom;
			
			xfrom = float(WIDTH)/VALUES*(i);
			yfrom = height - perbyte*m_speeds[j][i];
			
			yfrom += OFFSET;
			
			points[x] = QPoint(xfrom, yfrom);
			
			if(x != 0 && x < arsize)
			{
				points[x+1] = points[x];
				x += 2;
			}
			else
				x++;
		}
		
		painter->drawLines(points, arsize/2);
	}
}

