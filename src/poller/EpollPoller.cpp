/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 3 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#include "EpollPoller.h"
#include "RuntimeException.h"
#include <sys/epoll.h>
#include <errno.h>
#include <alloca.h>
#include <unistd.h>

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

int EpollPoller::handle()
{
	return m_epoll;
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

int EpollPoller::wait(int msec, Event* xev, int max)
{
	epoll_event* events = (epoll_event*) alloca(sizeof(epoll_event) * max);
	int nfds;
	
	nfds = epoll_wait(m_epoll, events, max, msec);
	
	for(int i=0;i<nfds;i++)
	{
		Event& ev = xev[i];
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
	}
	
	return nfds;
}
