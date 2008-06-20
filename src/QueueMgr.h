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

#ifndef _QUEUEMGR_H
#define _QUEUEMGR_H
#include <QThread>
#include <QTimer>
#include "Queue.h"
#include <QSettings>

class QueueMgr : public QObject
{
Q_OBJECT
public:
	QueueMgr();
	void exit();
	
	static QueueMgr* instance() { return m_instance; }
	
	inline int totalDown() const { return m_down; }
	inline int totalUp() const { return m_up; }
public slots:
	void doWork();
	void transferStateChanged(Transfer*,Transfer::State,Transfer::State);
private:
	static QueueMgr* m_instance;
	QTimer* m_timer;
	int m_nCycle;
	int m_down, m_up;
};

#endif
