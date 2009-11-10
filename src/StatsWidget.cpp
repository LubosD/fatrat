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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#include "StatsWidget.h"
#include "NetIface.h"
#include "QueueMgr.h"
#include "Settings.h"
#include <QPainter>
#include <QPaintEvent>
#include <QSettings>
#include <QtDebug>

const int SPACE = 15;

StatsWidget::StatsWidget(QWidget* parent) : QWidget(parent)
{
	m_globDown = m_globUp = -1;
	m_globDownPrev = m_globUpPrev = -1;
	
	refresh();
}

void StatsWidget::refresh()
{
	if(!isVisible() || size().isEmpty())
		return;
	
	QPair<qint64, qint64> newv = QPair<qint64, qint64>(-1, -1);
	m_strInterface = getRoutingInterface4();
	
	if(!m_strInterface.isEmpty())
		newv = getInterfaceStats(m_strInterface);
	
	m_globDownPrev = m_globDown;
	m_globUpPrev = m_globUp;
	m_globDown = newv.first;
	m_globUp = newv.second;

	update();
}

void StatsWidget::paintEvent(QPaintEvent* event)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	
	if(event != 0)
		painter.setClipRegion(event->region());
	
	if(m_globDownPrev >= 0 && m_globDown >= 0 && height() > SPACE*2)
	{
		int tmp;
		const int colwidth = width() / 8;
		const int colheight = height()-SPACE*2;
		qint64 mydown = QueueMgr::instance()->totalDown();
		qint64 myup = QueueMgr::instance()->totalUp();
		qint64 globdown = m_globDown - m_globDownPrev;
		qint64 globup = m_globUp - m_globUpPrev;
		const int maxdown = getSettingsValue("network/speed_down").toInt();
		const int maxup = getSettingsValue("network/speed_up").toInt();
		
		globdown = qMin<qint64>(globdown, maxdown);
		globup = qMin<qint64>(globup, maxup);
		
		mydown = qMin<qint64>(mydown, globdown);
		myup = qMin<qint64>(myup, globup);
		
		painter.fillRect(colwidth, SPACE, colwidth, colheight, Qt::white);
		painter.drawRect(colwidth, SPACE, colwidth, colheight);
		
		tmp = double(colheight)/maxdown*globdown;
		painter.fillRect(colwidth, colheight+SPACE, colwidth, -tmp, Qt::black);
		tmp = double(colheight)/maxdown*mydown;
		painter.fillRect(colwidth, colheight+SPACE, colwidth, -tmp, Qt::red);
		
		painter.fillRect(width()-colwidth, SPACE, -colwidth, colheight, Qt::white);
		painter.drawRect(width()-colwidth, SPACE, -colwidth, colheight);
		
		tmp = double(colheight)/maxup*globup;
		painter.fillRect(width()-colwidth, colheight+SPACE, -colwidth, -tmp, Qt::black);
		tmp = double(colheight)/maxup*myup;
		painter.fillRect(width()-colwidth, colheight+SPACE, -colwidth, -tmp, Qt::red);
		
		painter.drawText(QRect(0, 0, colwidth*3, SPACE), Qt::AlignCenter, "down");
		painter.drawText(QRect(width()-colwidth*3, 0, colwidth*3, SPACE), Qt::AlignCenter, "up");
		painter.drawText(QRect(0, height()-SPACE, width(), SPACE), Qt::AlignCenter, m_strInterface);
	}
}
