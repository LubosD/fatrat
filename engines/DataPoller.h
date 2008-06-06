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

#ifndef DATAPOLLER_H
#define DATAPOLLER_H
#include <QThread>
#include <QMap>
#include <QPair>
#include <sys/time.h>
#include "SocketInterface.h"

enum
{
	ERR_CONNECTION_ABORT = -4,
	ERR_CONNECTION_TIMEOUT,
	ERR_CONNECTION_ERR,
	ERR_CONNECTION_LOST
};

class DataPoller : public QThread
{
Q_OBJECT
public:
	DataPoller();
	~DataPoller();
	void run();
	
	void addSocket(SocketInterface* iface);
	void removeSocket(SocketInterface* iface);
	
	static DataPoller* instance() { return m_instance; }
protected:
	void processRead(SocketInterface* iface);
	void processWrite(SocketInterface* iface);
	void updateStats(const timeval& now, int msecs);
	static void updateStats(TransferStats& stats, int msecs);
private:
	int m_epoll;
	char* m_buffer;
	
	QMap<int, SocketInterface*> m_sockets;
	bool m_bAbort;
	
	static DataPoller* m_instance;
};

#endif
