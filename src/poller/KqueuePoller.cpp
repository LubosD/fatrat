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

#include "DebugLog.h"
#include "KqueuePoller.h"
#include "RuntimeException.h"
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <cstdlib>
#include <cerrno>
#ifdef __APPLE__
#include <unistd.h> // close
#endif

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
	// kqueue filters are distinct values, not bit flags, so each one needs its own kevent.
	// Filters that aren't requested are deleted, as this may be a modification of an existing registration.
	struct kevent ev[2];
	int eflags = EV_ADD | EV_ENABLE;
	
	if(flags & PollerOneShot)
		eflags |= EV_ONESHOT;
	
	EV_SET(&ev[0], socket, EVFILT_READ, (flags & PollerIn) ? eflags : EV_DELETE, 0, 0, 0);
	EV_SET(&ev[1], socket, EVFILT_WRITE, (flags & PollerOut) ? eflags : EV_DELETE, 0, 0, 0);
	
	for(int i=0;i<2;i++)
	{
		bool wanted = (i == 0) ? (flags & PollerIn) : (flags & PollerOut);
		int r = kevent(m_kqueue, &ev[i], 1, 0, 0, 0);
		int err = errno;
		FRDBG("kq addSocket fd=%d filter=%s %s flags=0x%x -> %d errno=%d", socket, i ? "WRITE" : "READ", wanted ? "ADD" : "DEL", flags, r, r == -1 ? err : 0);
		if(r == -1 && (wanted || err != ENOENT))
			return -1;
	}
	return 0;
}

int KqueuePoller::removeSocket(int socket)
{
	// Oneshot or closed sockets may already be gone, so ENOENT/EBADF are not errors
	struct kevent ev;
	
	EV_SET(&ev, socket, EVFILT_READ, EV_DELETE, 0, 0, 0);
	int r1 = kevent(m_kqueue, &ev, 1, 0, 0, 0);
	EV_SET(&ev, socket, EVFILT_WRITE, EV_DELETE, 0, 0, 0);
	int r2 = kevent(m_kqueue, &ev, 1, 0, 0, 0);
	FRDBG("kq removeSocket fd=%d read=%d write=%d", socket, r1, r2);
	return 0;
}

int KqueuePoller::wait(int msec, Event* ev, int max)
{
	struct kevent* evlist = (struct kevent*) alloca(sizeof(struct kevent) * max);
	struct timespec tspec = { msec/1000, (msec%1000)*1000000L };
	
	long long t0 = frDbgMs();
	int nev = kevent(m_kqueue, 0, 0, evlist, max, &tspec);
	FRDBG("kq wait(%d ms) -> %d events after %lld ms (kq=%d)", msec, nev, frDbgMs() - t0, m_kqueue);
	for(int i=0;i<nev;i++)
		FRDBG("  kq event ident=%d filter=%d flags=0x%x fflags=0x%x data=%lld", (int) evlist[i].ident, (int) evlist[i].filter, (unsigned) evlist[i].flags, (unsigned) evlist[i].fflags, (long long) evlist[i].data);
	for(int i=0;i<nev;i++)
	{
		Event event = { static_cast<int>(evlist[i].ident), 0 };
		
		if(evlist[i].flags & EV_ERROR)
			event.flags |= PollerError;
		if(evlist[i].flags & EV_EOF)
			event.flags |= PollerHup;
		if(evlist[i].filter == EVFILT_READ)
			event.flags |= PollerIn;
		if(evlist[i].filter == EVFILT_WRITE)
			event.flags |= PollerOut;
		
		ev[i] = event;
	}
	
	return nev;
}

