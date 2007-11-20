#include "HttpClient.h"
#include <QNetworkProxy>

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
		m_header.addValue("Referrer", referrer.toString());
	
	m_header.addValue("Host", host);
	m_header.addValue("User-Agent", "FatRat/" VERSION);
	m_header.addValue("Connection", "close");
}

void HttpEngine::request(QString file, bool, int)
{
	if(LimitedSocket::open(file, false))
	{
		m_nResume = m_file.pos();
		if(m_nSegmentEnd >= 0)
			m_header.addValue("Range", QString("bytes=%1-%2").arg(m_nResume).arg(m_nSegmentEnd));
		else
			m_header.addValue("Range", QString("bytes=%1-").arg(m_nResume));
		
		start();
	}
	else
		emit finished(true);
}

void HttpEngine::run()
{
	QTcpSocket* socket = new QTcpSocket;
	
	m_pRemote = socket;
	socket->setReadBufferSize(1024);
	
	try
	{
		if(!bindSocket(socket, m_bindAddress))
			throw tr("Failed to bind socket");
		
		if(m_proxyData.nType == Proxy::ProxyHttp)
			socket->connectToHost(m_proxyData.strIP,m_proxyData.nPort);
		else
		{
			if(m_proxyData.nType == Proxy::ProxySocks5)
			{
				QNetworkProxy proxy;
				
				proxy.setType(QNetworkProxy::Socks5Proxy);
				proxy.setHostName(m_proxyData.strIP);
				proxy.setPort(m_proxyData.nPort);
				proxy.setUser(m_proxyData.strUser);
				proxy.setPassword(m_proxyData.strPassword);
				
				socket->setProxy(proxy);
			}
			socket->connectToHost(m_url.host(),m_url.port(80));
		}
		
		if(!socket->waitForConnected())
			throw getErrorString(socket->error());
		if(m_bAbort)
			return;
		
		socket->write(m_header.toString().toAscii());
		socket->write("\r\n", 2);
		
		if(!socket->waitForBytesWritten())
			throw getErrorString(socket->error());
		
		if(m_bAbort)
			return;
		
		QByteArray response;
		
		while(true)
		{
			QByteArray line = readLine();
			
			qDebug() << "Received HTTP line" << line;
			
			if(line.size() <= 2)
			{
				if(line.isEmpty())
					throw getErrorString(socket->error());
				else
					break;
			}
			else
				response += line;
		}
		
		QHttpResponseHeader header = QHttpResponseHeader(QString(response));
		
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
				m_nToTransfer = header.value("content-length").toULongLong();
				emit receivedSize(m_nToTransfer+m_nResume);
			}
			
			dataCycle(bChunked);
			emit finished(false);
			
			break;
		}
		case 301:
		case 302: // redirect
			emit redirected(header.value("location"));
			break;
		default: // error
			throw header.reasonPhrase();
		}
	}
	catch(const QString& text)
	{
		m_strError = text;
		emit finished(true);
	}
	
	m_file.close();
	
	if(socket)
		doClose(&socket);
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
