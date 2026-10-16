/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 3 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#include "TorrentProgressWidget.h"
#include "TorrentPieceMap.h"
#include <QPainter>
#include <algorithm>
#include <QtDebug>

TorrentProgressWidget::TorrentProgressWidget(QWidget* parent) : QWidget(parent)
{
	m_data = new quint32[1000];
}

TorrentProgressWidget::~TorrentProgressWidget()
{
	delete [] m_data;
}

void TorrentProgressWidget::generate(const libtorrent::bitfield& data)
{
	m_image = generate(data, 1000, m_data);
	update();
}

void TorrentProgressWidget::generate(const std::vector<int>& data)
{
	m_image = generate(data, 1000, m_data);
	update();
}

// blue colored
QImage TorrentProgressWidget::generate(const libtorrent::bitfield& data, int width, quint32* buf, float sstart, float send)
{
	TorrentPieceMap::renderPieces(data, width, buf, sstart, send);
	return QImage((uchar*) buf, width, 1, QImage::Format_RGB32);
}

// grey colored
QImage TorrentProgressWidget::generate(const std::vector<int>& data, int width, quint32* buf, float sstart, float send)
{
	TorrentPieceMap::renderAvailability(data, width, buf, sstart, send);
	return QImage((uchar*) buf, width, 1, QImage::Format_RGB32);
}

void TorrentProgressWidget::paintEvent(QPaintEvent* event)
{
	QPainter painter;
	painter.begin(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setClipRegion(event->region());
	
	QImage bdraw = m_image.scaled(size());
	painter.drawImage(0, 0, bdraw);
	
	painter.end();
}
