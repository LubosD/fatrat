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

#ifndef TORRENTPROGRESSWIDGET_H
#define TORRENTPROGRESSWIDGET_H
#include <QWidget>
#include <QImage>
#include <QPaintEvent>
#include <cmath>
#include <cstring>
#include <libtorrent/bitfield.hpp>

class TorrentProgressWidget : public QWidget
{
Q_OBJECT
public:
	TorrentProgressWidget(QWidget* parent);
	~TorrentProgressWidget();
	
	void generate(const libtorrent::bitfield& data);
	void generate(const std::vector<int>& data);
	
	// blue colored
	static QImage generate(const libtorrent::bitfield& data, int width, quint32* buf, float sstart = 0, float send = 0);
	// grey colored
	static QImage generate(const std::vector<int>& data, int width, quint32* buf, float sstart = 0, float send = -1);
	
	void paintEvent(QPaintEvent* event);
private:
	QImage m_image;
	quint32* m_data;
};

#endif
