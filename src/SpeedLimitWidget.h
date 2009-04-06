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

#ifndef SPEEDLIMITWIDGET_H
#define SPEEDLIMITWIDGET_H
#include <QWidget>
#include <QLabel>
#include <QTimer>

class RightClickLabel : public QLabel
{
Q_OBJECT
public:
	RightClickLabel(QWidget* parent) : QLabel(parent), m_nSpeed(0), m_bUpload(false)
	{
	}
	void setUpload(bool is) { m_bUpload = is; }
	void refresh(int speed) { m_nSpeed = speed; }
public slots:
	void setLimit();
protected:
	void mousePressEvent(QMouseEvent* event);
	void mouseDoubleClickEvent(QMouseEvent* event);
	
	int m_nSpeed;
	bool m_bUpload;
};

#include "ui_SpeedLimitWidget.h"

class SpeedLimitWidget : public QWidget, Ui_SpeedLimitWidget
{
Q_OBJECT
public:
	SpeedLimitWidget(QWidget* parent);
public slots:
	void refresh();
protected:
	void mouseDoubleClickEvent(QMouseEvent*);
private:
	QTimer m_timer;
};

#endif
