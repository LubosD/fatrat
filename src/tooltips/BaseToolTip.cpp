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

#include "BaseToolTip.h"
#include <QApplication>
#include <QDesktopWidget>

BaseToolTip::BaseToolTip(QObject* master, QWidget* parent)
	: QLabel(parent)
{
	setWindowFlags(Qt::ToolTip);
	setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
	setLineWidth(1);

	if(master != 0)
		connect(master, SIGNAL(destroyed(QObject*)), this, SLOT(deleteLater()));
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(doRefresh()));
	
	m_timer.start(1000);
	QMetaObject::invokeMethod(this, "doRefresh", Qt::QueuedConnection);
}

void BaseToolTip::mousePressEvent(QMouseEvent*)
{
	hide();
}

void BaseToolTip::placeMe() // from Qt
{
	QPoint pos = QCursor::pos();
	int s = QApplication::desktop()->screenNumber(pos);
	QRect screen = QApplication::desktop()->screenGeometry(s);
	
	QPoint p = pos;
	p += QPoint(2, 16);
	if (p.x() + this->width() > screen.x() + screen.width())
		p.rx() -= 4 + this->width();
	if (p.y() + this->height() > screen.y() + screen.height())
		p.ry() -= 24 + this->height();
	if (p.y() < screen.y())
		p.setY(screen.y());
	if (p.x() + this->width() > screen.x() + screen.width())
		p.setX(screen.x() + screen.width() - this->width());
	if (p.x() < screen.x())
		p.setX(screen.x());
	if (p.y() + this->height() > screen.y() + screen.height())
		p.setY(screen.y() + screen.height() - this->height());
	
	move(p);
}
