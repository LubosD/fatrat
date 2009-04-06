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

#ifndef _INFOBAR_H
#define _INFOBAR_H
#include <QWidget>
#include <QLabel>
#include <QTimer>
#include "Transfer.h"

class InfoBar : public QLabel
{
Q_OBJECT
public:
	InfoBar(QWidget* parent, Transfer* d);
	virtual ~InfoBar();

	void mousePressEvent(QMouseEvent* event);
	//void mouseReleaseEvent(QMouseEvent* event);
	void mouseMoveEvent(QMouseEvent* event);
	
	static InfoBar* getInfoBar(Transfer* d);
	static void removeAll();
public slots:
	void downloadDestroyed(QObject*);
	void refresh();
private:
	int m_mx,m_my;
	Transfer* m_download;
	QTimer m_timer;
};

#endif
