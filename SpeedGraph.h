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
*/

#ifndef _SPEEDGRAPH_H
#define _SPEEDGRAPH_H
#include <QWidget>
#include <QPainter>
#include <QPaintEvent>
#include <QTimer>
#include "Transfer.h"

class SpeedGraph : public QWidget
{
Q_OBJECT
public:
	SpeedGraph(QWidget* parent);
	void setRenderSource(Transfer* t);
public slots:
	void setNull() { setRenderSource(0); }
	void saveScreenshot();
protected:
	virtual void paintEvent(QPaintEvent* event);
	virtual void contextMenuEvent(QContextMenuEvent* event);
	void draw(QPaintDevice* device, QPaintEvent* event = 0);
	void drawNoData(QPainter& painter);
	
	Transfer* m_transfer;
	QTimer* m_timer;
};

#endif
