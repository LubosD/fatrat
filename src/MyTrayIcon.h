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

#ifndef MYTRAYICON_H
#define MYTRAYICON_H
#include "tooltips/TrayToolTip.h"
#include <QtDebug>

class MyTrayIcon : public QSystemTrayIcon
{
public:
	MyTrayIcon(QWidget* parent) : QSystemTrayIcon(parent)
	{
		m_tip = new TrayToolTip;
	}
	
	~MyTrayIcon()
	{
		delete m_tip;
	}
	
	bool event(QEvent* e)
	{
		if(e->type() == QEvent::ToolTip)
		{
			m_tip->regMove();
			return true;
		}
		else
			return QSystemTrayIcon::event(e);
	}
private:
	TrayToolTip* m_tip;
};

#endif
