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

#include "DataPoller.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

static const int MAX_SPEED_SAMPLES = 5;
static const int SOCKET_TIMEOUT = 15; // timeout ∊ <SOCKET_TIMEOUT, SOCKET_TIMEOUT+1)
static const int BUFFER_SIZE = 4096;
static const int MAX_EVENTS = 30;

DataPoller* DataPoller::m_instance = 0;

DataPoller::DataPoller() : m_bAbort(false)
{
	m_instance = this;
	m_buffer = new char[BUFFER_SIZE];
	m_epoll = epoll_create(10);
	
	start();
}

DataPoller::~DataPoller()
{
	m_bAbort = true;
	wait();
	delete [] m_buffer;
	close(m_epoll);
}

void DataPoller::run()
{
	timeval lastT, nowT;
	int wait = 500; // 500ms at max, so that socket timeouts are detected
	
	gettimeofday(&lastT, 0);
	
	while(!m_bAbort)
	{
		epoll_event events[MAX_EVENTS];
		int num;
		
		// wait for the next planned operation or socket event
		num = epoll_wait(m_epoll, events, MAX_EVENTS, wait);
		gettimeofday(&nowT, 0);
		
		for(int i=0;i<num;i++)
		{
			SocketInterface* iface = m_sockets[events[num].data.fd];
			
			try
			{
				if(events[i].events & EPOLLERR)
					throw ERR_CONNECTION_ERR;
				else if(events[i].events & EPOLLHUP)
					throw ERR_CONNECTION_LOST;
				else if(events[i].events & EPOLLIN && !iface->m_downloadStats.hasNextProcess())
					processRead(iface);
				else if(events[i].events & EPOLLOUT && !iface->m_uploadStats.hasNextProcess())
					processWrite(iface);
			}
			catch(int errn)
			{
				removeSocket(iface);
				iface->error(errn);
			}
		}
		
		// run planned reads/writes
		for(QMap<int, SocketInterface*>::iterator it = m_sockets.begin(); it != m_sockets.end();)
		{
			try
			{
				if(it.value()->m_downloadStats.nextProcessDue(nowT))
					processRead(it.value());
				if(it.value()->m_uploadStats.nextProcessDue(nowT))
					processWrite(it.value());
				it++;
			}
			catch(int errn)
			{
				epoll_ctl(m_epoll, EPOLL_CTL_DEL, it.value()->socket(), 0);
				it.value()->error(errn);
				it = m_sockets.erase(it);
			}
		}
		
		// plan next reads/writes
		wait = 500; // 500ms at max, so that socket timeouts are detected
		for(QMap<int, SocketInterface*>::const_iterator it = m_sockets.constBegin(); it != m_sockets.constEnd(); it++)
		{
			if(it.value()->m_downloadStats.hasNextProcess())
			{
				int diff = it.value()->m_downloadStats.nextProcess - nowT;
				if(diff < wait)
					wait = diff;
			}
			if(it.value()->m_uploadStats.hasNextProcess())
			{
				int diff = it.value()->m_uploadStats.nextProcess - nowT;
				if(diff < wait)
					wait = diff;
			}
		}
		
		// update the statistics every second
		int msecs = nowT-lastT;
		if(msecs >= 1000)
		{
			updateStats(nowT, msecs);
			lastT = nowT;
		}
	}
}

void DataPoller::updateStats(const timeval& nowT, int msecs)
{
	for(QMap<int, SocketInterface*>::iterator it = m_sockets.begin(); it != m_sockets.end();)
	{
		updateStats(it.value()->m_downloadStats, msecs);
		updateStats(it.value()->m_uploadStats, msecs);
		
		if(nowT.tv_sec - it.value()->m_lastProcess.tv_sec > SOCKET_TIMEOUT)
		{
			epoll_ctl(m_epoll, EPOLL_CTL_DEL, it.value()->socket(), 0);
			it.value()->error(ERR_CONNECTION_TIMEOUT);
			it = m_sockets.erase(it);
		}
		else
			it++;
	}
}

void DataPoller::updateStats(TransferStats& stats, int msecs)
{
	qint64 bytes = stats.transferred - stats.transferredPrev;
	
	if(!bytes)
		return;
	
	stats.speedData << QPair<int,qint64>(msecs, bytes);
	
	if(stats.speedData.size() > MAX_SPEED_SAMPLES)
		stats.speedData.removeFirst();
	
	qint64 tTime = 0;
	bytes = 0; // reusing the variable
	for(int j=0;j<stats.speedData.size();j++)
	{
		tTime += stats.speedData[j].first;
		bytes += stats.speedData[j].second;
	}
	
	stats.speed = bytes/tTime*1000;
	stats.transferredPrev = stats.transferred;
}

void DataPoller::addSocket(SocketInterface* iface)
{
	int socket = iface->socket();
	epoll_event ev;
	
	m_sockets[socket] = iface;
	
	ev.data.fd = socket;
	ev.events = EPOLLHUP | EPOLLERR | EPOLLOUT | EPOLLIN;
	
	epoll_ctl(m_epoll, EPOLL_CTL_ADD, socket, &ev);
}

void DataPoller::removeSocket(SocketInterface* iface)
{
	int sock = iface->socket();
	
	m_sockets.remove(sock);
	epoll_ctl(m_epoll, EPOLL_CTL_DEL, sock, 0);
}

void DataPoller::processRead(SocketInterface* iface)
{
	qint64 thisCall = 0, thisLimit = 0;
	int downL, upL;
	
	iface->speedLimit(downL, upL);
	
	if(downL)
		thisLimit = downL / 2;
	
	while(thisCall < thisLimit || !thisLimit)
	{
		ssize_t r = read(iface->socket(), m_buffer, qMin<ssize_t>(BUFFER_SIZE, thisLimit - thisCall));
		
		if(r < 0)
		{
			if(errno == EAGAIN)
				break;
			else
				throw errno;
		}
		else if(r == 0)
			throw ERR_CONNECTION_LOST;
		
		if(!iface->putData(m_buffer, r))
			throw ERR_CONNECTION_ABORT;
		
		iface->m_downloadStats.transferred += r;
		thisCall += r;
	}
	
	gettimeofday(&iface->m_lastProcess, 0);
	if(!thisCall || !thisLimit)
		memset(&iface->m_downloadStats.nextProcess, 0, sizeof(timeval));
	else if(thisLimit)
	{
		unsigned long msec = thisCall * 1000 / downL;
		iface->m_downloadStats.nextProcess = iface->m_lastProcess;
		iface->m_downloadStats.nextProcess.tv_usec += msec*1000;
		iface->m_downloadStats.nextProcess.tv_sec += iface->m_downloadStats.nextProcess.tv_usec / 1000;
		iface->m_downloadStats.nextProcess.tv_usec %= 1000000LL;
	}
}

void DataPoller::processWrite(SocketInterface* iface)
{
	qint64 thisCall = 0, thisLimit = 0;
	int downL, upL;
	
	iface->speedLimit(downL, upL);
	
	if(downL)
		thisLimit = upL / 2;
	
	while(thisCall < thisLimit || !thisLimit)
	{
		unsigned long bytes = qMin<unsigned long>(BUFFER_SIZE, thisLimit - thisCall);
		
		if(iface->m_leftover.isEmpty())
		{
			if(!iface->getData(m_buffer, &bytes))
				throw ERR_CONNECTION_ABORT;
		}
		else
		{
			bytes = qMin<ssize_t>(bytes, iface->m_leftover.size());
			memcpy(m_buffer, iface->m_leftover.constData(), bytes);
			iface->m_leftover = iface->m_leftover.mid(bytes);
		}
		
		if(!bytes)
			break;
		
		int r = write(iface->socket(), m_buffer, bytes);
		if(r < 0)
		{
			if(errno == EAGAIN)
			{
				iface->m_leftover = QByteArray(m_buffer, bytes);
				break;
			}
			else
				throw errno;
		}
		else if(r == 0)
			throw ERR_CONNECTION_LOST;
		else if(r < int(bytes))
		{
			iface->m_leftover = QByteArray(m_buffer+r, bytes-r);
			break;
		}
		
		iface->m_uploadStats.transferred += bytes;
		thisCall += bytes;
	}
	
	gettimeofday(&iface->m_lastProcess, 0);
	if(!thisCall || !thisLimit)
		memset(&iface->m_uploadStats.nextProcess, 0, sizeof(timeval));
	else if(thisLimit)
	{
		unsigned long msec = thisCall * 1000 / downL;
		iface->m_uploadStats.nextProcess = iface->m_lastProcess;
		iface->m_uploadStats.nextProcess.tv_usec += msec*1000;
		iface->m_uploadStats.nextProcess.tv_sec += iface->m_uploadStats.nextProcess.tv_usec / 1000;
		iface->m_uploadStats.nextProcess.tv_usec %= 1000000LL;
	}
}
