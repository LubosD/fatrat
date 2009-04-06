/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation
with the OpenSSL special exemption.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "InfoBar.h"
#include <QMouseEvent>
#include <QReadWriteLock>
#include <QList>
#include "fatrat.h"

static QReadWriteLock m_lock;
static QList<InfoBar*> m_bars;

InfoBar::InfoBar(QWidget* parent, Transfer* d) : QLabel(parent), m_download(d)
{
	setWindowFlags(Qt::ToolTip);
	setFrameStyle(QFrame::Box | QFrame::Plain);
	setLineWidth(1);
	
	connect(d, SIGNAL(destroyed(QObject*)), this, SLOT(downloadDestroyed(QObject*)));
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(refresh()));
	
	m_timer.start(1000);
	
	m_lock.lockForWrite();
	m_bars << this;
	m_lock.unlock();
	
	refresh();
	move(QCursor::pos()-QPoint(5,5));
	show();
}

InfoBar::~InfoBar()
{
	m_lock.lockForWrite();
	m_bars.removeAll(this);
	m_lock.unlock();
}

void InfoBar::refresh()
{
	if(!m_download)
		return;
	
	qulonglong done = m_download->done(), total = m_download->total();
	QString pc, speed, time = "- - -";
	QString name = m_download->name();
	
	if(name.length() > 30)
	{
		name.resize(30);
		name += "...";
	}
	
	if(m_download->isActive())
	{
		QString s;
		Transfer::Mode mode = m_download->primaryMode();
		int down,up;
		m_download->speeds(down,up);
		
		if(down || m_download->mode() == Transfer::Download)
		{
			speed = QString("[%1] <b>d</b>").arg(formatSize(down, true));
			
			if(mode == Transfer::Download && total && down)
				time = formatTime((total-done)/down);
		}
		if(up || m_download->mode() == Transfer::Upload)
		{
			speed += QString(" [%1] <b>u</b>").arg(formatSize(up, true));
			
			if(mode == Transfer::Upload && total && up)
				time = formatTime((total-done)/up);
		}
	}
	if(speed.isEmpty())
			speed = "- - -";
	if(total)
		pc = QString("%1").arg(100.0/total*done, 0, 'f', 1);
	
	setText( QString("<font color=blue>N:</font> %1 | <font color=blue>P:</font> %2% | "
			"<font color=blue>S:</font> %3 | <font color=blue>T:</font> %4").arg(name).arg(pc).arg(speed).arg(time) );
	
	QSize size = sizeHint();
	size.setWidth(std::max(size.width(),350));
	resize(size);
}

void InfoBar::mousePressEvent(QMouseEvent* event)
{
	if(event->buttons() == Qt::LeftButton)
	{
		m_mx = event->globalX();
		m_my = event->globalY();
	}
	else if(event->buttons() == Qt::MidButton)
		delete this;
}

void InfoBar::mouseMoveEvent(QMouseEvent* event)
{
	if(event->buttons() == Qt::LeftButton)
	{
		move(x()+event->globalX()-m_mx, y()+event->globalY()-m_my);
		mousePressEvent(event);
	}
}

void InfoBar::downloadDestroyed(QObject*)
{
	m_download = 0;
	close();
	deleteLater();
}

void InfoBar::removeAll()
{
	qDeleteAll(m_bars);
}

InfoBar* InfoBar::getInfoBar(Transfer* d)
{
	InfoBar* instance = 0;
	m_lock.lockForRead();
	
	foreach(InfoBar* b, m_bars)
	{
		if(b->m_download == d)
		{
			instance = b;
			break;
		}
	}
	
	m_lock.unlock();
	return instance;
}
