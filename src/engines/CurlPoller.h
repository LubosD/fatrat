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


#ifndef CURL_POLLER
#define CURL_POLLER
#include <QThread>
#include <QMutex>
#include <QMap>
#include <QHash>
#include <curl/curl.h>
#include "engines/CurlUser.h"
#include "poller/Poller.h"

class CurlPollingMaster;

class CurlPoller : public QThread
{
public:
	CurlPoller();
	~CurlPoller();
	
	static void init();
	
	void addTransfer(CurlUser* obj);
	void removeTransfer(CurlUser* obj);
	void addTransfer(CurlPollingMaster* obj);
	void removeTransfer(CurlPollingMaster* obj);
	
	void run();
	void pollingCycle(bool oneshot);
	
	static CurlPoller* instance() { return m_instance; }
protected:
	void epollEnable(int socket, int events);
	static int socket_callback(CURL* easy, curl_socket_t s, int action, CurlPoller* This, void* socketp);
	static int timer_callback(CURLM* multi, long newtimeout, long* timeout);
protected:
	static CurlPoller* m_instance;
	bool m_bAbort;
	CURLM* m_curlm;
	Poller* m_poller;
	int m_curlTimeout;
	long m_timeout;
	
	typedef QHash<int, QPair<int,CurlStat*> > sockets_hash;
	
	QMap<CURL*, CurlUser*> m_users;
	QMap<int, CurlPollingMaster*> m_masters;
	sockets_hash m_sockets;
	QMutex m_usersLock;
	
	QList<int> m_socketsToRemove;
	sockets_hash m_socketsToAdd;
};

#endif
