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

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#ifndef _POLLER_H
#define _POLLER_H
#include <QObject>
#include <QList>

class Poller : public QObject
{
public:
	static Poller* createInstance(QObject* parent = 0);
	
	enum Flags { PollerIn = 1, PollerOut = 2, PollerError = 4, PollerHup = 8, PollerOneShot = 16 };
	
	struct Event
	{
		int socket;
		int flags;
	};
	
	virtual int handle() = 0;
	
	// Should perform modify if the socket is already in the set
	// provided that the underlying subsystem requires that.
	virtual int addSocket(int socket, int flags) = 0;
	virtual int removeSocket(int socket) = 0;
	virtual int wait(int msec, Event* ev, int max) = 0;
protected:
	Poller(QObject* parent);
};

#endif
