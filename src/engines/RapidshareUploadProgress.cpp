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

#include "RapidshareUploadProgress.h"
#include "RapidshareUpload.h"
#include "Settings.h"
#include <QPainter>
#include <QPaintEvent>

RSProgressWidget::RSProgressWidget(QWidget* parent)
	: QWidget(parent), m_upload(0)
{
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(update()));
	m_timer.start(getSettingsValue("gui_refresh").toInt());
}

void RSProgressWidget::setTransfer(RapidshareUpload* obj)
{
	m_upload = obj;
	update();
}

void RSProgressWidget::paintEvent(QPaintEvent* event)
{
	if(!m_upload)
		return;
	
	QPainter painter(this);
	const int width = this->width();
	const int chunk = m_upload->chunkSize();
	
	painter.setClipRegion(event->region());
	painter.fillRect(0, 0, width, 30, QBrush(Qt::white));
	painter.setPen(QPen(Qt::black, 2));
	
	float ratio = float(width)/m_upload->m_nTotal;
	
	if(m_upload->isActive())
		painter.fillRect(0, 0, m_upload->done()*ratio, 30, Qt::blue);
	painter.fillRect(0, 0, m_upload->m_nDone*ratio, 30, Qt::green);
	
	for(qint64 i=chunk;i<m_upload->m_nTotal;i+=chunk)
		painter.drawLine(i*ratio, 0, i*ratio, 30);
	painter.drawRect(0, 0, width, 30);
}
