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
#include "CurlUser.h"

class CurlPoller : public QThread
{
public:
	CurlPoller();
	~CurlPoller();
	
	void addTransfer(CurlUser* obj);
	void removeTransfer(CurlUser* obj);
	
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
	int m_epoll;
	
	QMap<CURL*, CurlUser*> m_users;
	QHash<int, QPair<int,CurlUser*> > m_sockets;
	QMutex m_usersLock;
};

#endif
