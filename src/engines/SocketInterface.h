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

#ifndef SOCKETINTERFACE_H
#define SOCKETINTERFACE_H
#include <sys/time.h>
#include <QList>
#include <QPair>
#include <QByteArray>
#include <QHostAddress>

struct TransferStats
{
	TransferStats();
	bool hasNextProcess() const;
	bool nextProcessDue(const timeval& now) const;
	
	timeval nextProcess; // next reception/send
	qint64 transferred, transferredPrev;
	QList<QPair<int, qint64> > speedData; // msec, bytes
	int speed;
};

class SocketInterface
{
public:
	SocketInterface();
	virtual ~SocketInterface();
	
	void clear();
	
	void connectTo(QHostAddress addr, unsigned short port);
	virtual void speedLimit(int& down, int& up) const { down = up = 0; }
	virtual bool putData(const char* data, unsigned long bytes) = 0;
	virtual bool getData(char* data, unsigned long* bytes) = 0;
	virtual void error(int error) = 0;
	
	void getSpeed(int& down, int& up) const;
	void setBlocking(bool blocking = false);
	int socket() { return m_socket; }
protected:
	void setWantWrite();
	
	timeval m_lastProcess;
	TransferStats m_downloadStats, m_uploadStats;
	QByteArray m_leftover;
	int m_socket;
	
	friend class DataPoller;
};

bool operator<(const timeval& t1, const timeval& t2);
int operator-(const timeval& t1, const timeval& t2);

#endif
