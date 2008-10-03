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

#include "EpollPoller.h"
#include "RuntimeException.h"
#include <sys/epoll.h>
#include <errno.h>

EpollPoller::EpollPoller(QObject* parent)
	: Poller(parent)
{
	m_epoll = epoll_create(20);
	if(m_epoll <= 0)
		throw RuntimeException("epoll_create() failed");
}

EpollPoller::~EpollPoller()
{
	if(m_epoll > 0)
		close(m_epoll);
}

int EpollPoller::addSocket(int socket, int flags)
{
	epoll_event event;
	event.events = 0;
	event.data.fd = socket;
	
	if(flags & PollerIn)
		event.events |= EPOLLIN;
	if(flags & PollerOut)
		event.events |= EPOLLOUT;
	if(flags & PollerOneShot)
		event.events |= EPOLLONESHOT;
	if(flags & PollerHup)
		event.events |= EPOLLHUP;
	
	if(epoll_ctl(m_epoll, EPOLL_CTL_MOD, socket, &event))
	{
		if(errno == ENOENT)
		{
			if(epoll_ctl(m_epoll, EPOLL_CTL_ADD, socket, &event))
				return errno;
		}
		else
			return errno;
	}
	
	return 0;
}

int EpollPoller::removeSocket(int socket)
{
	epoll_ctl(m_epoll, EPOLL_CTL_DEL, socket, 0);
	return errno;
}

QList<Poller::Event> EpollPoller::wait(int msec)
{
	epoll_event events[20];
	int nfds;
	QList<Event> retval;
	
	nfds = epoll_wait(m_epoll, events, sizeof(events)/sizeof(events[0]), msec);
	
	for(int i=0;i<nfds;i++)
	{
		Event ev;
		ev.socket = events[i].data.fd;
		ev.flags = 0;
		
		if(events[i].events & EPOLLIN)
			ev.flags |= PollerIn;
		if(events[i].events & EPOLLOUT)
			ev.flags |= PollerOut;
		if(events[i].events & EPOLLERR)
			ev.flags |= PollerError;
		if(events[i].events & EPOLLHUP)
			ev.flags |= PollerHup;
		
		retval << ev;
	}
	
	return retval;
}
