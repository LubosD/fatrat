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

#include "HttpClient.h"
#include <QNetworkProxy>
#include <QSslSocket>

HttpEngine::HttpEngine(QUrl url, QUrl referrer, QUuid proxyUuid) : m_pRemote(0), m_url(url)
{
	QString user_info;
	QString query, host;
	QList<Proxy> listProxy = Proxy::loadProxys();
	m_proxyData.nType = Proxy::ProxyNone;
	
	host = url.host();
	if(url.port(80) != 80)
		host += QString(":") + QString::number(url.port(80));
	
	foreach(Proxy p,listProxy)
	{
		if(p.uuid == proxyUuid)
		{
			m_proxyData = p;
			break;
		}
	}
	
	if(!url.hasQuery())
		query = url.path();
	else
		query = url.path()+"?"+url.encodedQuery();
	
	if(query.isEmpty())
		query = "/";
	
	if(m_proxyData.nType == Proxy::ProxyHttp)
	{
		m_header.setRequest("GET", QString("%1://%2%3").arg(url.scheme()).arg(host).arg(query));
		if(!m_proxyData.strUser.isEmpty())
			m_header.addValue("Proxy-Authorization", QString("Basic %1").arg((QString) (m_proxyData.strUser+":"+m_proxyData.strPassword).toUtf8().toBase64()) );
	}
	else
		m_header.setRequest("GET", query);
	
	user_info = url.userInfo();
	if(!user_info.isEmpty())
		m_header.addValue("Authorization", QString("Basic %1").arg( QString(user_info.toUtf8().toBase64()) ));
	
	if(referrer.isValid())
		m_header.addValue("Referer", referrer.toString());
	
	m_header.addValue("Host", host);
	m_header.addValue("User-Agent", "FatRat/" VERSION);
	m_header.addValue("Connection", "close");
}

void HttpEngine::request(QString file, bool bUpload, int)
{
	if(LimitedSocket::open(file, bUpload))
	{
		// when we're uploading, it's fully up to the class' user to add special request headers etc.
		if(!bUpload)
		{
			m_nResume = m_file.pos();
			if(m_nSegmentEnd >= 0)
				m_header.addValue("Range", QString("bytes=%1-%2").arg(m_nResume).arg(m_nSegmentEnd));
			else //if(m_nResume)
				m_header.addValue("Range", QString("bytes=%1-").arg(m_nResume));
		}
		else
		{
			if(m_nSegmentStart < 0)
				m_nToTransfer = m_file.size();
			else
				m_nToTransfer = m_nSegmentEnd - m_nSegmentStart;
			m_header.addValue("Content-Length", QString::number(m_nToTransfer + m_strHeader.size() + m_strFooter.size()));
			m_header.setRequest("POST", m_header.path());
		}
		
		QString cookies;
		for(QMap<QString,QString>::const_iterator it = m_mapCookies.constBegin(); it != m_mapCookies.constEnd(); it++)
		{
			if(!cookies.isEmpty())
				cookies += "; ";
			cookies += it.key();
			cookies += '=';
			cookies += it.value();
		}
		if(!cookies.isEmpty())
			m_header.addValue("Cookie", cookies);
		
		start();
	}
	else
		emit finished(true);
}

void HttpEngine::run()
{
	try
	{
		if(!bindSocket(m_pRemote, m_bindAddress))
			throw tr("Failed to bind socket");
		
		if(m_proxyData.nType == Proxy::ProxyHttp)
		{
			m_pRemote = new QTcpSocket;
			
			if(!bindSocket(m_pRemote, m_bindAddress))
				throw tr("Failed to bind socket");
			
			m_pRemote->connectToHost(m_proxyData.strIP,m_proxyData.nPort);
		}
		else
		{
			bool bEncrypted = m_url.scheme() == "https";
			
			QSslSocket* sslsock;
			QTcpSocket* normsock;
			
			if(!bEncrypted)
				m_pRemote = normsock = new QTcpSocket;
			else
				m_pRemote = sslsock = new QSslSocket;
			
			if(m_proxyData.nType == Proxy::ProxySocks5)
			{
				QNetworkProxy proxy;
				
				proxy.setType(QNetworkProxy::Socks5Proxy);
				proxy.setHostName(m_proxyData.strIP);
				proxy.setPort(m_proxyData.nPort);
				proxy.setUser(m_proxyData.strUser);
				proxy.setPassword(m_proxyData.strPassword);
				
				m_pRemote->setProxy(proxy);
			}
			
			if(!bEncrypted)
			{
				//normsock->connectToHost(m_url.host(),m_url.port(80));
				connectToHost(normsock, m_url.host(),m_url.port(80));
			}
			else
			{
				connect(sslsock, SIGNAL(sslErrors(const QList<QSslError> &)), sslsock, SLOT(ignoreSslErrors()));
				//sslsock->connectToHostEncrypted(m_url.host(),m_url.port(443));
				connectToHost(sslsock, m_url.host(), m_url.port(443));
			}
		}
		
		m_pRemote->setReadBufferSize(1024);
		
		if(!m_pRemote->waitForConnected())
			throw getErrorString(m_pRemote->error());
		if(m_bAbort)
			return;
		
		m_pRemote->write(m_header.toString().toAscii());
		if(m_strHeader.isEmpty())
			m_pRemote->write("\r\n", 2);
		m_pRemote->write(m_strHeader);
		
		if(!m_pRemote->waitForBytesWritten())
			throw getErrorString(m_pRemote->error());
		
		if(m_bAbort)
			return;
		
		statusMessage(tr("Sending data"));
		performUpload();
		
		if(m_bAbort)
			return;
		
		if(!m_strFooter.isEmpty())
		{
			m_pRemote->write(m_strFooter);
			if(!m_pRemote->waitForBytesWritten())
				throw getErrorString(m_pRemote->error());
		}
		
		statusMessage(tr("Receiving data"));
		processServerResponse();
	}
	catch(const QString& text)
	{
		m_strError = text;
		emit finished(true);
	}
	
	m_file.close();
	
	if(m_pRemote)
		doClose(&m_pRemote);
}

void HttpEngine::processServerResponse()
{
	QByteArray response;
	
	while(true)
	{
		QByteArray line = readLine();
		
		if(line.size() <= 2)
		{
			if(line.isEmpty())
				throw getErrorString(m_pRemote->error());
			else
				break;
		}
		else
			response += line;
	}
	
	QHttpResponseHeader header = QHttpResponseHeader(QString(response));
	
	if(!m_bUpload)
		handleDownloadHeaders(header);
	else
		handleUploadHeaders(header);
}

void HttpEngine::performUpload()
{
	if(!m_bUpload)
		return;
	
	while(!m_bAbort && m_nTransfered < m_nToTransfer)
	{
		bool success;
		
		success = writeCycle();
		
		if(!success)
			throw getErrorString(m_pRemote->error());
	}
}

void HttpEngine::handleUploadHeaders(QHttpResponseHeader header)
{
	int code = header.statusCode();
	if(code >= 400)
		throw header.reasonPhrase();
	
	if(code >= 300 && code < 400)
		emit redirected(header.value("location"));
	
	qint64 clen = header.value("content-length").toLongLong();
	while(m_strResponse.size() < clen)
		m_strResponse += m_pRemote->read(1024);
	
	emit finished(false);
}

void HttpEngine::handleDownloadHeaders(QHttpResponseHeader header)
{
	QList<QPair<QString,QString> > cs = header.values();
	for(int i=0;i<cs.size();i++)
	{
		if(cs[i].first.compare("set-cookie", Qt::CaseInsensitive))
			continue;
			
		QString& c = cs[i].second;
		int p = c.indexOf(';');
			
		if(p < 0)
			continue;
		c.resize(p);
			
		p = c.indexOf('=');
		if(p < 0)
			continue;
		
		qDebug() << c;
			
		m_mapCookies[c.mid(0, p)] = c.mid(p+1);
	}
	
	switch(header.statusCode())
	{
	case 200:
	{
		if(m_file.pos() && !header.hasKey("content-range"))
		{
			if(m_nSegmentStart < 0)
			{
				// resume not supported
				m_file.resize(0);
				m_file.seek(0);
				
				emit statusMessage(tr("Resume not supported"));
			}
			else
				throw tr("Segmentation not supported by server");
		}
	}
	case 206: // data will follow
	{
		bool bChunked = false;
		if(header.value("transfer-encoding").compare("chunked",Qt::CaseInsensitive) == 0)
			bChunked = true;
		else
		{
			m_nToTransfer = header.value("content-length").toLongLong();
			emit receivedSize(m_nToTransfer+m_nResume);
		}
		
		if(header.hasKey("content-disposition"))
		{
			QString disp = header.value("content-disposition");
			int pos = disp.indexOf("filename=");
			
			if(pos != -1)
			{
				QString name = disp.mid(pos+9);
				
				if(name.startsWith('"') && name.endsWith('"'))
					name = name.mid(1, name.size()-2);
				emit renamed(name);
			}
		}
		
		dataCycle(bChunked);
		emit finished(false);
		
		break;
	}
	case 301 ... 399: // redirect
		::sleep(1);
		emit redirected(header.value("location"));
		break;
	case 416: // Requested Range Not Satisfiable
		m_strError = tr("Requested Range Not Satisfiable - the file has probably already been downloaded");
		emit finished(false);
		break;
	default: // error
		throw header.reasonPhrase();
	}
}

void HttpEngine::dataCycle(bool bChunked)
{
	bool bOK = true;
	do
	{
		m_nTransfered = 0;
		
		if(bChunked)
		{
			QString line;
			do
			{
				line = readLine();
			}
			while(line.trimmed().isEmpty());
			
			m_nToTransfer = line.toULongLong(0,16);
			if(!m_nToTransfer)
				break;
		}
		
		while(!m_bAbort && (m_nTransfered < m_nToTransfer || !m_nToTransfer))
		{
			if(! (bOK = readCycle()))
			{
				if(m_nToTransfer != 0)
					throw getErrorString(m_pRemote->error());
				break;
			}
		}
	}
	while(bChunked && bOK && !m_bAbort);
}
