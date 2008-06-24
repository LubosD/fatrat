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

#include "SocketInterface.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

SocketInterface::SocketInterface()
	: m_socket(0)
{
	memset(&m_lastProcess, 0, sizeof m_lastProcess);
}

SocketInterface::~SocketInterface()
{
	clear();
}

void SocketInterface::clear()
{
	if(m_socket > 0)
	{
		close(m_socket);
		m_socket = 0;
	}
	memset(&m_lastProcess, 0, sizeof m_lastProcess);
}

void SocketInterface::setWantWrite()
{
	gettimeofday(&m_uploadStats.nextProcess, 0);
}

void SocketInterface::getSpeed(int& down, int& up) const
{
	down = m_downloadStats.speed;
	up = m_uploadStats.speed;
}

void SocketInterface::setBlocking(bool blocking)
{
	int arg = fcntl(m_socket, F_GETFL);
	
	if(blocking)
		arg &= ~O_NONBLOCK;
	else
		arg |= O_NONBLOCK;
	
	fcntl(m_socket, F_SETFL, arg);
}

void SocketInterface::connectTo(QHostAddress addr, unsigned short port)
{
	if(m_socket > 0)
		close(m_socket);
	
	try
	{
		if(addr.protocol() == QAbstractSocket::IPv6Protocol)
		{
			sockaddr_in6 sin6;
			m_socket = ::socket(PF_INET6, SOCK_STREAM, 0);
			
			if(m_socket < 0)
				throw errno;
			
			memset(&sin6, 0, sizeof sin6);
			sin6.sin6_family = AF_INET6;
			sin6.sin6_port = htons(port);
			
			Q_IPV6ADDR a = addr.toIPv6Address();
			memcpy(&sin6.sin6_addr, &a, 16);
			
			setBlocking(false);
			
			if(::connect(m_socket, (sockaddr*) &sin6, sizeof sin6) < 0 && errno != EINPROGRESS)
				throw errno;
		}
		else if(addr.protocol() == QAbstractSocket::IPv4Protocol)
		{
			sockaddr_in sin;
			m_socket = ::socket(PF_INET, SOCK_STREAM, 0);
			
			if(m_socket < 0)
				throw errno;
			
			sin.sin_family = AF_INET;
			sin.sin_port = htons(port);
			sin.sin_addr.s_addr = addr.toIPv4Address();
			
			setBlocking(false);
			
			if(::connect(m_socket, (sockaddr*) &sin, sizeof sin) < 0 && errno != EINPROGRESS)
				throw errno;
		}
		else
			throw EAFNOSUPPORT;
	}
	catch(int err)
	{
		this->error(err);
	}
}

//////////////////////////////////////////////////////

TransferStats::TransferStats() : transferred(0), transferredPrev(0), speed(0)
{
	memset(&nextProcess, 0, sizeof nextProcess);
}

bool TransferStats::hasNextProcess() const
{
	return nextProcess.tv_sec || nextProcess.tv_usec;
}

bool TransferStats::nextProcessDue(const timeval& now) const
{
	return (nextProcess < now) && hasNextProcess();
}

int operator-(const timeval& t1, const timeval& t2)
{
	return (t1.tv_sec-t2.tv_sec)*1000 + (t1.tv_usec-t2.tv_usec)/1000;
}
