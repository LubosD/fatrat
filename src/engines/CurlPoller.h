/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2010 Lubos Dolezel <lubos a dolezel.info>

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


#ifndef CURL_POLLER
#define CURL_POLLER
#include <QThread>
#include <QMutex>
#include <QMap>
#include <QHash>
#include <QQueue>
#include <curl/curl.h>
#include "engines/CurlUser.h"
#include "poller/Poller.h"

class CurlPoller : public QThread
{
public:
	CurlPoller();
	~CurlPoller();
	
	void addTransfer(CurlUser* obj);
	void removeTransfer(CurlUser* obj);
	// so that CURL objects don't get destroyed while there is an active multi call
	void addForSafeDeletion(CURL* curl);
	
	void run();
	
	static CurlPoller* instance() { return m_instance; }
protected:
	void epollEnable(int socket, int events);
	static int socket_callback(CURL* easy, curl_socket_t s, int action, CurlPoller* This, void* socketp);
	static int timer_callback(CURLM* multi, long newtimeout, long* timeout);
private:
	static CurlPoller* m_instance;
	bool m_bAbort;
	CURLM* m_curlm;
	Poller* m_poller;
	
	typedef QHash<int, QPair<int,CurlUser*> > sockets_hash;
	
	QMap<CURL*, CurlUser*> m_users;
	sockets_hash m_sockets;
	QMutex m_usersLock;
	QQueue<CURL*> m_queueToDelete;
	
	QList<int> m_socketsToRemove;
	sockets_hash m_socketsToAdd;
};

#endif
