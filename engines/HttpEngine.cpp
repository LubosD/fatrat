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

#include "fatrat.h"
#include "HttpEngine.h"
#include "RuntimeException.h"
#include <sys/types.h>
#include <sys/socket.h>

HttpEngine2::HttpEngine2()
	: m_limitDown(0), m_limitUp(0), m_socket(0), m_state(StateNone)
{
	
}

HttpEngine2::~HttpEngine2()
{
}

void HttpEngine2::get(QUrl url, QString file, QString referrer, qint64 from, qint64 to)
{
	QString host, query;
	m_url = url;
	
	m_file.setFileName(file);
	if(!m_file.open(QIODevice::WriteOnly))
		throw RuntimeException(m_file.errorString());
	m_file.seek(from);
	
	if(!url.hasQuery())
		query = url.path();
	else
		query = url.path()+"?"+url.encodedQuery();
	
	if(query.isEmpty())
		query = "/";
	
	m_request.setRequest("GET", query);
	
	if(url.port(80) != 80)
		host = QString("%1:%2").arg(url.host()).arg(url.port(80));
	else
		host = url.host();
	
	if(!referrer.isEmpty())
		m_request.addValue("Referer", referrer);
	m_request.addValue("Host", host);
	m_request.addValue("User-Agent", "FatRat/" VERSION);
	m_request.addValue("Connection", "close");
	
	QHostInfo::lookupHost(url.host(), this, SLOT(domainResolved(QHostInfo)));
}

void HttpEngine2::domainResolved(QHostInfo info)
{
	QList<QHostAddress> addresses, ip4, ip6;
	
	if(info.error() != QHostInfo::NoError)
		;
	
	addresses = info.addresses();
	
	foreach(QHostAddress addr, addresses)
	{
		if(addr.protocol() == QAbstractSocket::IPv6Protocol)
			ip6 << addr;
		else if(addr.protocol() == QAbstractSocket::IPv4Protocol)
			ip4 << addr;
	}
	
	addresses.clear();
	addresses << ip6 << ip4;
	
	if(addresses.isEmpty())
		;
	
	// Let's try the first IP address
	m_state = StateConnecting;
}

void HttpEngine2::setSpeedLimit(int down, int up)
{
	m_limitDown = down;
	m_limitUp = up;
}

int HttpEngine2::socket() const
{
	return m_socket;
}

void HttpEngine2::speedLimit(int& down, int& up) const
{
	down = m_limitDown;
	up = m_limitUp;
}

bool HttpEngine2::putData(const char* data, unsigned long bytes)
{
}

bool HttpEngine2::getData(char* data, unsigned long* bytes)
{
}

void HttpEngine2::error(int error)
{
}

