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

struct TransferInfo;

static const int ERR_CONNECTION_LOST = -1;
static const int ERR_CONNECTION_ERR = -2;
static const int ERR_CONNECTION_TIMEOUT = -3;

class DataPoller : public QThread
{
Q_OBJECT
public:
	DataPoller();
	~DataPoller();
	void run();
	
	void addSocket(int socket, int file, bool bUpload, qint64 toTransfer);
	void removeSocket(int sock);
	int processRead(TransferInfo& info);
	int processWrite(TransferInfo& info);
	qint64 getSpeed(int sock);
	qint64 getProgress(int sock);
	void setSpeedLimit(int sock, qint64 limit);
	
	static DataPoller* instance() { return m_instance; }
signals:
	void processingDone(int socket, int error);
private:
	int m_epoll;
	
	QMap<int, TransferInfo> m_sockets;
	bool m_bAbort;
	
	static DataPoller* m_instance;
};

struct TransferInfo
{
	bool bUpload;
	int socket, file;
	qint64 toTransfer;
	qint64 speedLimit;
	
	// internal
	timeval lastProcess; // last reception/send
	qint64 transferred, transferredPrev;
	QList<QPair<int, qint64> > speedData; // msec, bytes
	qint64 speed;
};

#endif
