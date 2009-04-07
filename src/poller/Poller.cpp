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

#include "config.h"
#if defined(HAVE_SYS_EPOLL_H)
#	include "EpollPoller.h"
#elif defined(HAVE_KQUEUE)
#	include "KqueuePoller.h"
#endif

#include "Poller.h"

Poller* Poller::createInstance(QObject* parent)
{
#if defined(HAVE_SYS_EPOLL_H)
	return new EpollPoller(parent);
#elif defined(HAVE_KQUEUE)
	return new KqueuePoller(parent);
#else
#	error Your OS is unsupported as there is no polling implementation written for it.
#endif
}

Poller::Poller(QObject* parent)
	: QObject(parent)
{
}

