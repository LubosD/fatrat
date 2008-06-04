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
struct TransferStats;
class SocketInterface;

static const int ERR_CONNECTION_LOST = -1;
static const int ERR_CONNECTION_ERR = -2;
static const int ERR_CONNECTION_TIMEOUT = -3;
static const int ERR_CONNECTION_ABORT = -4;

class DataPoller : public QThread
{
Q_OBJECT
public:
	DataPoller();
	~DataPoller();
	void run();
	
	void addSocket(SocketInterface* iface);
	void removeSocket(SocketInterface* iface);
	int processRead(TransferInfo& info);
	int processWrite(TransferInfo& info);
	void getSpeed(SocketInterface* iface, qint64& down, qint64& up) const;
	static void updateStats(TransferStats& stats, qint64 usecs);
	
	static DataPoller* instance() { return m_instance; }
private:
	int m_epoll;
	char* m_buffer;
	
	QMap<int, TransferInfo> m_sockets;
	bool m_bAbort;
	
	static DataPoller* m_instance;
};

class SocketInterface
{
public:
	virtual int socket() const = 0;
	virtual void speedLimit(int& down, int& up) const { down = up = 0; }
	virtual bool putData(const char* data, unsigned long bytes) { return true; }
	virtual bool getData(char* data, unsigned long* bytes) { *bytes = 0; return true; }
	virtual void error(int error) = 0;
};

struct TransferStats
{
	TransferStats();
	
	timeval lastProcess; // last reception/send
	qint64 transferred, transferredPrev;
	QList<QPair<int, qint64> > speedData; // msec, bytes
	qint64 speed;
};

struct TransferInfo
{
	SocketInterface* interface;
	
	// internal
	TransferStats downloadStats, uploadStats;
	
	QByteArray leftover;
};

#endif
