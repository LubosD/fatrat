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

#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H
#include <QTcpSocket>
#include <QHttpRequestHeader>
#include <QMap>
#include "LimitedSocket.h"

class HttpClient : public LimitedSocket
{
Q_OBJECT
public:
	HttpClient(QUrl url, QUrl referrer, QUuid proxy);
	void addHeaderValue(QString name, QString value) { m_header.addValue(name, value); }
	
	// uploads only
	void setRequestBody(QByteArray hdr, QByteArray footer) { m_strHeader = hdr; m_strFooter = footer; }
	QByteArray getResponseBody() const { return m_strResponse; }
	
	virtual void request(QString file, bool bUpload, int);
	virtual void run();
	virtual void setRemoteName(QString) { }
	virtual QIODevice* getRemote() { return m_pRemote; }
	
	QMap<QString, QString>& getCookies() { return m_mapCookies; }
signals:
	void redirected(QString newurl);
	void renamed(QString dispName);
private:
	void processServerResponse();
	void dataCycle(bool bChunked);
	void handleDownloadHeaders(QHttpResponseHeader headers);
	void handleUploadHeaders(QHttpResponseHeader headers);
	void performUpload();
	
	QTcpSocket* m_pRemote;
	QUrl m_url;
	QHttpRequestHeader m_header;
	Proxy m_proxyData;
	QByteArray m_strHeader, m_strFooter, m_strResponse;
	QMap<QString, QString> m_mapCookies;
};

#endif
