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
#include "DataPoller.h"
#include "RuntimeException.h"
#include <sys/types.h>
#include <sys/socket.h>

HttpEngine::HttpEngine()
	: m_limitDown(0), m_limitUp(0), m_state(StateNone)
{
	
}

HttpEngine::~HttpEngine()
{
	clear();
}

void HttpEngine::clear()
{
	DataPoller::instance()->removeSocket(this);
	SocketInterface::clear();
	m_strResponse.clear();
	if(m_file.isOpen())
		m_file.close();
}

void HttpEngine::get(QUrl url, QString file, QString referrer, qint64 from, qint64 to)
{
	QString host, query;
	m_url = url;
	
	clear();
	
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
	
	m_mode = ModeGet;
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

void HttpEngine::domainResolved(QHostInfo info)
{
	QList<QHostAddress> ip4, ip6;
	
	if(info.error() != QHostInfo::NoError)
		reportError(tr("Cannot resolve the domain name:")+' '+info.errorString());
	
	m_addresses = info.addresses();
	
	foreach(QHostAddress addr, m_addresses)
	{
		if(addr.protocol() == QAbstractSocket::IPv6Protocol)
			ip6 << addr;
		else if(addr.protocol() == QAbstractSocket::IPv4Protocol)
			ip4 << addr;
	}
	
	m_addresses.clear();
	m_addresses << ip6 << ip4;
	
	if(m_addresses.isEmpty())
		reportError(tr("The domain name cannot be resolved to any addresses"));
	
	QByteArray headers = m_request.toString().toUtf8();
	m_outputBuffer.putData(headers.constData(), headers.size());
	
	// Let's try the first IP address
	m_state = StateConnecting;
	connectTo(m_addresses.front(), m_url.port(80));
	m_addresses.pop_front();
	DataPoller::instance()->addSocket(this);
}

void HttpEngine::setSpeedLimit(int down, int up)
{
	m_limitDown = down;
	m_limitUp = up;
}

int HttpEngine::socket() const
{
	return m_socket;
}

void HttpEngine::speedLimit(int& down, int& up) const
{
	down = m_limitDown;
	up = m_limitUp;
}

bool HttpEngine::putData(const char* data, unsigned long bytes)
{
	if(m_state == StateReceiving) // response body
	{
		
	}
	else if(m_state != StateNone) // HTTP headers
	{
		m_lineFeeder.feed(data, bytes);
		
		QByteArray line;
		while(m_lineFeeder.getLine(&line))
		{
			if(line.isEmpty())
			{
				m_state = StateReceiving;
				break;
			}
			
			m_strResponse += line + "\r\n";
		}
		
		if(m_state == StateReceiving)
		{
			QHttpResponseHeader hdr(m_strResponse);
			QByteArray residue;
			
			m_strResponse.clear();
			
			processResponse(hdr);
			residue = m_lineFeeder.residue();
			
			if(!residue.isEmpty())
				this->putData(residue.constData(), residue.size());
		}
	}
	
	return true;
}

bool HttpEngine::getData(char* data, unsigned long* bytes)
{
	if(m_state == StateConnecting)
	{
		// We're now connected
		m_state = StateRequesting;
	}
	
	if(m_state == StateRequesting)
	{
		if(!m_outputBuffer.isEmpty()) // send more HTTP headers
			m_outputBuffer.getData(data, bytes);
		else
		{
			if(m_mode == ModeGet)
			{
				m_state = StatePending;
				*bytes = 0;
			}
			else
				m_state = StateSending;
		}
	}
	
	if(m_state == StateSending)
	{
		// TODO: the POST data
	}
	
	return true;
}

void HttpEngine::processResponse(QHttpResponseHeader& hdr)
{
	switch(hdr.statusCode())
	{
	case 200 ... 299:
		// everything went fine
		break;
	case 300 ... 399:
		// redirect
		break;
	default:
		;// errors
	}
}

void HttpEngine::error(int error)
{
	if(m_state == StateConnecting)
	{
		if(!m_addresses.isEmpty())
		{
			connectTo(m_addresses.front(), m_url.port(80));
			m_addresses.pop_front();
			DataPoller::instance()->addSocket(this);
		}
		else
		{
			reportError(tr("Couldn't connect to the server"));
		}
	}
}

void HttpEngine::reportError(QString error)
{
	// TODO
}
