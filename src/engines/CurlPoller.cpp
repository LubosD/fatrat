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


#include "CurlPoller.h"
#include <sys/epoll.h>
#include <errno.h>

CurlPoller* CurlPoller::m_instance = 0;

CurlPoller::CurlPoller()
	: m_bAbort(false)
{
	if(m_instance)
		abort();
	
	curl_global_init(CURL_GLOBAL_SSL);
	m_curlm = curl_multi_init();
	m_epoll = epoll_create(20);
	
	curl_multi_setopt(m_curlm, CURLMOPT_SOCKETFUNCTION, socket_callback);
	curl_multi_setopt(m_curlm, CURLMOPT_SOCKETDATA, this);
	
	m_instance = this;
	start();
}

CurlPoller::~CurlPoller()
{
	m_bAbort = true;
	
	if(isRunning())
		wait();
	
	m_instance = 0;
	curl_multi_cleanup(m_curlm);
	curl_global_cleanup();
	close(m_epoll);
}

void CurlPoller::run()
{
	while(!m_bAbort)
	{
		epoll_event events[20];
		int nfds, dummy;
		long timeout;
		
		curl_multi_timeout(m_curlm, &timeout);
		
		nfds = epoll_wait(m_epoll, events, 10, timeout);
		
		if(!nfds)
			curl_multi_perform(m_curlm, &dummy);
		
		for(int i=0;i<nfds;i++)
		{
			int mask = 0;
			
			if(events[i].events & EPOLLIN)
				mask |= CURL_CSELECT_IN;
			if(events[i].events & EPOLLOUT)
				mask |= CURL_CSELECT_OUT;
			if(events[i].events & (EPOLLERR | EPOLLHUP))
				mask |= CURL_CSELECT_ERR;
			
			curl_multi_socket_action(m_curlm, events[i].data.fd, mask, &dummy);
		}
		
		QMutexLocker locker(&m_usersLock);
		while(CURLMsg* msg = curl_multi_info_read(m_curlm, &dummy))
		{
			if(msg->msg != CURLMSG_DONE)
				continue;
			
			CurlUser* user = m_users[msg->easy_handle];
			
			if(user)
				user->transferDone(msg->data.result);
		}
	}
}

int CurlPoller::socket_callback(CURL* easy, curl_socket_t s, int action, CurlPoller* This, void* socketp)
{
	epoll_event event;
	event.events = EPOLLERR | EPOLLHUP;
	event.data.fd = s;
	
	if(action == CURL_POLL_IN || action == CURL_POLL_INOUT)
		event.events |= EPOLLIN;
	if(action == CURL_POLL_OUT || action == CURL_POLL_INOUT)
		event.events |= EPOLLOUT;
	
	if(action == CURL_POLL_REMOVE)
		epoll_ctl(This->m_epoll, EPOLL_CTL_DEL, s, 0);
	else
	{
		if(epoll_ctl(This->m_epoll, EPOLL_CTL_ADD, s, &event))
		{
			if(errno == EEXIST)
			{
				if(epoll_ctl(This->m_epoll, EPOLL_CTL_MOD, s, &event))
					return errno;
			}
			else
				return errno;
		}
	}
	
	return 0;
}

void CurlPoller::addTransfer(CurlUser* obj)
{
	QMutexLocker locker(&m_usersLock);
	
	CURL* handle = obj->curlHandle();
	m_users[handle] = obj;
	curl_multi_add_handle(m_curlm, handle);
}

void CurlPoller::removeTransfer(CurlUser* obj)
{
	QMutexLocker locker(&m_usersLock);
	
	CURL* handle = obj->curlHandle();
	curl_multi_remove_handle(m_curlm, handle);
	m_users.remove(handle);
}
