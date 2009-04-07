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

#ifndef MAINTAB_H
#define MAINTAB_H
#include <QMenu>
#include <QTabWidget>
#include <QTabBar>
#include <QContextMenuEvent>
#include <QToolButton>

class MainTab : public QTabWidget
{
Q_OBJECT
public:
	MainTab(QWidget* parent);
public slots:
	void closeTab();
	void closeTabBtn();
	void closeAllTabs();
	void openAppTool();
	void changeTabTitle(QString newTitle);
	void currentTabChanged(int newTab);
protected:
	void initAppTools(QMenu* tabOpenMenu);
	void contextMenuEvent(QContextMenuEvent* event);
	void mousePressEvent(QMouseEvent* event);
	
	int m_index;
	int m_lastIndex, m_lastIndexCur;
	QToolButton* m_toolTabClose;
};

#endif
