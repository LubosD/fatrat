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

#ifndef HTTPENGINE_H
#define HTTPENGINE_H
#include "DataPoller.h"
#include <QString>
#include <QFile>
#include <QUrl>
#include <QHostInfo>
#include <QHttpRequestHeader>

class HttpEngine2 : public QObject, public SocketInterface
{
Q_OBJECT
public:
	HttpEngine2();
	~HttpEngine2();
	
	void get(QUrl url, QString file, QString referrer = QString(), qint64 from = 0, qint64 to = -1);
	void setSpeedLimit(int down, int up);
	
	virtual int socket() const;
	virtual void speedLimit(int& down, int& up) const;
	virtual bool putData(const char* data, unsigned long bytes);
	virtual bool getData(char* data, unsigned long* bytes);
	virtual void error(int error);
public slots:
	void domainResolved(QHostInfo info);
private:
	int m_limitDown, m_limitUp;
	int m_socket;
	QFile m_file;
	QUrl m_url;
	
	enum HttpState { StateConnecting, StateRequesting, StateSending, StatePending, StateReceiving, StateNone };
	HttpState m_state;
	
	QHttpRequestHeader m_request;
};

#endif
