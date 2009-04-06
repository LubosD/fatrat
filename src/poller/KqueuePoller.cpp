/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation
with the OpenSSL special exemption.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "KqueuePoller.h"
#include "RuntimeException.h"
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

KqueuePoller::KqueuePoller(QObject* parent)
	: Poller(parent)
{
	m_kqueue = kqueue();
	if(m_kqueue <= 0)
		throw RuntimeException("kqueue() failed");
}

KqueuePoller::~KqueuePoller()
{
	if(m_kqueue > 0)
		close(m_kqueue);
}

int KqueuePoller::handle()
{
	return m_kqueue;
}

int KqueuePoller::addSocket(int socket, int flags)
{
	struct kevent ev;
	int eflags = EV_ADD | EV_ENABLE;
	int efilters = 0;
	
	if(flags & PollerIn)
		efilters |= EVFILT_READ;
	if(flags & PollerOut)
		efilters |= EVFILT_WRITE;
	if(flags & PollerOneShot)
		eflags |= EV_ONESHOT;
	if(flags & PollerHup)
		eflags |= EV_EOF;
	
	EV_SET(&ev, socket, efilters, eflags, 0, 0, 0);
	return kevent(m_kqueue, &ev, 1, 0, 0, 0);
}

int KqueuePoller::removeSocket(int socket)
{
	struct kevent ev;
	EV_SET(&ev, socket, 0, EV_DELETE | EV_DISABLE, 0, 0, 0);
	return kevent(m_kqueue, &ev, 1, 0, 0, 0);
}

QList<Poller::Event> KqueuePoller::wait(int msec)
{
	QList<Event> retval;
	struct kevent evlist[30];
	struct timespec tspec = { msec/1000, (msec%1000)*1000000L };
	
	int nev = kevent(m_kqueue, 0, 0, evlist, sizeof(evlist)/sizeof(evlist[0]), &tspec);
	for(int i=0;i<nev;i++)
	{
		Event event = { evlist[i].ident, 0 };
		
		if(evlist[i].flags & EV_ERROR)
			event.flags |= PollerError;
		if(evlist[i].flags & EV_EOF)
			event.flags |= PollerHup;
		if(evlist[i].filter & EVFILT_READ)
			event.flags |= PollerIn;
		if(evlist[i].filter & EVFILT_WRITE)
			event.flags |= PollerOut;
		
		retval << event;
	}
	
	return retval;
}

