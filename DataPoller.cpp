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
static const int SOCKET_TIMEOUT = 15;
static const int BUFFER_SIZE = 4096;
static const qint64 MILLION = 1000000LL;

DataPoller* DataPoller::m_instance = 0;

TransferStats::TransferStats() : transferred(0), transferredPrev(0), speed(0)
{
	gettimeofday(&lastProcess, 0);
}

bool operator<(const timeval& t1, const timeval& t2)
{
	if(t1.tv_sec < t2.tv_sec)
		return true;
	else if(t1.tv_sec > t2.tv_sec)
		return false;
	else
		return t1.tv_usec < t2.tv_usec;
}

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
	
	gettimeofday(&lastT, 0);
	
	while(!m_bAbort)
	{
		epoll_event events[20];
		int num = epoll_wait(m_epoll, events, sizeof(events) / sizeof(events[0]), 500);
		
		for(int i=0;i<num;i++)
		{
			int errn = 0;
			TransferInfo& info = m_sockets[events[num].data.fd];
			
			if(events[i].events & EPOLLERR)
				errn = ERR_CONNECTION_ERR;
			else if(events[i].events & EPOLLHUP)
				errn = ERR_CONNECTION_LOST;
			else if(events[i].events & EPOLLIN)
				errn = processRead(info);
			else if(events[i].events & EPOLLOUT)
				errn = processWrite(info);
			
			if(errn != 0)
			{
				removeSocket(info.interface);
				info.interface->error(errn);
			}
		}
		
		gettimeofday(&nowT, 0);
		
		qint64 usecs = qint64(nowT.tv_sec-lastT.tv_sec)*MILLION + nowT.tv_usec-lastT.tv_usec;
		if(usecs < MILLION)
			continue;
		
		for(QMap<int, TransferInfo>::iterator it = m_sockets.begin(); it != m_sockets.end();)
		{
			updateStats(it->downloadStats, usecs);
			updateStats(it->uploadStats, usecs);
			
			timeval lastProcess = std::max<timeval>(it->downloadStats.lastProcess, it->uploadStats.lastProcess);
			
			if(nowT.tv_sec - lastProcess.tv_sec > SOCKET_TIMEOUT)
			{
				epoll_ctl(m_epoll, EPOLL_CTL_DEL, it->interface->socket(), 0);
				it->interface->error(ERR_CONNECTION_TIMEOUT);
				it = m_sockets.erase(it);
				continue;
			}
			it++;
		}
		
		lastT = nowT;
	}
}

void DataPoller::updateStats(TransferStats& stats, qint64 usecs)
{
	qint64 bytes = stats.transferred - stats.transferredPrev;
	
	if(!bytes)
		return;
	
	stats.speedData << QPair<int,qint64>(usecs/1000, bytes);
	
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

void DataPoller::getSpeed(SocketInterface* iface, qint64& down, qint64& up) const
{
	down = m_sockets[iface->socket()].downloadStats.speed;
	up = m_sockets[iface->socket()].uploadStats.speed;
}

void DataPoller::addSocket(SocketInterface* iface)
{
	int socket = iface->socket();
	TransferInfo& info = m_sockets[socket];
	epoll_event ev;
	
	ev.data.fd = socket;
	
	int arg = fcntl(socket, F_GETFL);
	fcntl(socket, F_SETFL, arg | O_NONBLOCK);
	
	ev.events = EPOLLHUP | EPOLLERR | EPOLLOUT | EPOLLIN;
	
	epoll_ctl(m_epoll, EPOLL_CTL_ADD, socket, &ev);
	info.interface = iface;
}

void DataPoller::removeSocket(SocketInterface* iface)
{
	int sock = iface->socket();
	
	m_sockets.remove(sock);
	epoll_ctl(m_epoll, EPOLL_CTL_DEL, sock, 0);
}

int DataPoller::processRead(TransferInfo& info)
{
	qint64 thisCall = 0, thisLimit = 0;
	timeval tv;
	
	gettimeofday(&tv, 0);
	
	int downL, upL;
	info.interface->speedLimit(downL, upL);
	if(downL)
	{
		thisLimit = (tv.tv_sec-info.downloadStats.lastProcess.tv_sec)*downL;
		thisLimit += qint64(tv.tv_usec-info.downloadStats.lastProcess.tv_usec)*double(downL)/MILLION;
	}
	
	while(thisCall < thisLimit || !thisLimit)
	{
		ssize_t r = read(info.interface->socket(), m_buffer, qMin<ssize_t>(BUFFER_SIZE, thisLimit - thisCall));
		
		if(r < 0)
		{
			if(errno == EAGAIN)
				break;
			else
				return errno;
		}
		else if(r == 0)
			return -1;
		
		if(!info.interface->putData(m_buffer, r))
			return -4;
		
		info.downloadStats.transferred += r;
		thisCall += r;
	}
	
	gettimeofday(&info.downloadStats.lastProcess, 0);
	return 0;
}

int DataPoller::processWrite(TransferInfo& info)
{
	qint64 thisCall = 0, thisLimit = 0;
	timeval tv;
	
	gettimeofday(&tv, 0);
	
	int downL, upL;
	info.interface->speedLimit(downL, upL);
	if(downL)
	{
		thisLimit = (tv.tv_sec-info.uploadStats.lastProcess.tv_sec)*upL;
		thisLimit += qint64(tv.tv_usec-info.uploadStats.lastProcess.tv_usec)*double(upL)/MILLION;
	}
	
	while(thisCall < thisLimit || !thisLimit)
	{
		unsigned long bytes = qMin<unsigned long>(BUFFER_SIZE, thisLimit - thisCall);
		
		if(info.leftover.isEmpty())
		{
			if(!info.interface->getData(m_buffer, &bytes))
				return -4;
		}
		else
		{
			bytes = qMin<ssize_t>(bytes, info.leftover.size());
			memcpy(m_buffer, info.leftover.constData(), bytes);
			info.leftover = info.leftover.mid(bytes);
		}
		
		int r = write(info.interface->socket(), m_buffer, bytes);
		if(r < 0)
		{
			if(errno == EAGAIN)
			{
				info.leftover = QByteArray(m_buffer, bytes);
				break;
			}
			else
				return errno;
		}
		else if(r == 0)
			return -1;
		else if(r < int(bytes))
		{
			info.leftover = QByteArray(m_buffer+r, bytes-r);
			break;
		}
		
		info.uploadStats.transferred += bytes;
		thisCall += bytes;
	}
	
	gettimeofday(&info.uploadStats.lastProcess, 0);
	return 0;
}
