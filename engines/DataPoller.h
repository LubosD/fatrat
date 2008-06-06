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
	void getSpeed(SocketInterface* iface, qint64& down, qint64& up) const;
	
	static DataPoller* instance() { return m_instance; }
protected:
	void processRead(TransferInfo& info);
	void processWrite(TransferInfo& info);
	void updateStats(const timeval& now, int msecs);
	static void updateStats(TransferStats& stats, int msecs);
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
	virtual bool putData(const char* data, unsigned long bytes) = 0;
	virtual bool getData(char* data, unsigned long* bytes) = 0;
	virtual void error(int error) = 0;
};

struct TransferStats
{
	TransferStats();
	bool hasNextProcess() const;
	bool nextProcessDue(const timeval& now) const;
	
	timeval nextProcess; // next reception/send
	qint64 transferred, transferredPrev;
	QList<QPair<int, qint64> > speedData; // msec, bytes
	qint64 speed;
};

struct TransferInfo
{
	SocketInterface* interface;
	
	// internal
	timeval lastProcess;
	TransferStats downloadStats, uploadStats;
	QByteArray leftover;
};

#endif
