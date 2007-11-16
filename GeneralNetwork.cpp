#include "GeneralNetwork.h"
#include <iostream>
#include <QtDebug>
#include <QHostAddress>
#include <QNetworkProxy>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <QFileInfo>

using namespace std;

const int SPEED_SAMPLES = 7;
static quint16 m_ftpPort = 50000;

HttpEngine::HttpEngine(QUrl url, QUrl referrer, QUuid proxyUuid) : m_url(url)
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

void HttpEngine::request(QString file)
{
	if(m_pSocket)
	{
		delete m_pSocket;
		m_pSocket = 0;
	}
	
	if(LimitedSocket::request(file))
	{
		m_nResume = m_file.pos();
		m_header.addValue("Range", QString("bytes=%1-").arg(m_nResume));
		start();
	}
	else
		emit finished(this,true);
}

void HttpEngine::run()
{
	m_pSocket = new QTcpSocket;
	m_pSocket->setReadBufferSize(1024);
	
	if(!bindSocket(m_pSocket, m_bindAddress))
	{
		m_strError = tr("Failed to bind socket");
		doClose(m_pSocket);
		emit finished(this,true);
		return;
	}
	
	if(m_proxyData.nType == Proxy::ProxyHttp)
		m_pSocket->connectToHost(m_proxyData.strIP,m_proxyData.nPort);
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
			
			m_pSocket->setProxy(proxy);
		}
		m_pSocket->connectToHost(m_url.host(),m_url.port(80));
	}
	
	if(!m_pSocket->waitForConnected())
	{
		emit finished(this,true);
		doClose(m_pSocket);
		return;
	}
	if(m_bAbort)
		return;
	
	m_pSocket->write(m_header.toString().toAscii());
	m_pSocket->write("\r\n", 2);
	
	if(!m_pSocket->waitForBytesWritten())
	{
		doClose(m_pSocket);
		emit finished(this,true);
		return;
	}
	if(m_bAbort)
		return;
	
	QByteArray response;
	QString text;
	
	while(true)
	{
		QByteArray line = readLine();
		
		qDebug() << "Received HTTP line" << line;
		
		if(line.size() <= 2)
		{
			cout << "Line len is " << line.size() << endl;
			if(line.isEmpty())
			{
				m_strError = tr("Timeout");
				emit finished(this,true);
				doClose(m_pSocket);
				return;
			}
			else
				break;
		}
		else
			response += line;
	}
	
	text = response;
	
	QHttpResponseHeader header(text);
	
	//qDebug() << "Processed the header";
	emit responseReceived(header);
	
	switch(header.statusCode())
	{
	case 200:
	{
		if(m_file.pos() && !header.hasKey("content-range"))
		{
			// resume not supported
			m_file.resize(0);
			m_file.seek(0);
		}
	}
	case 206: // data will follow
	{
		bool bChunked = false;
		if(header.value("transfer-encoding").compare("chunked",Qt::CaseInsensitive) == 0)
			bChunked = true;
		
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
			else
				m_nToTransfer = header.value("content-length").toULongLong();
			
			while(!m_bAbort && (m_nTransfered < m_nToTransfer || !m_nToTransfer))
			{
				bOK = readCycle();
				if(!bOK)
					break;
			}
		}
		while(bChunked && bOK);
		
		if(!m_nToTransfer)
			bOK = true;
		
		m_file.close();
		if(!m_bAbort)
			emit finished(this,!bOK);
		else
			m_bAbort = false;
		
		break;
	}
	case 301:
	case 302: // redirect
		break;
	default: // error
		m_strError = header.reasonPhrase();
		emit finished(this,true);
	}
	
	if(m_pSocket)
		doClose(m_pSocket);
}

bool LimitedSocket::bindSocket(QAbstractSocket* sock, QHostAddress addr)
{
	if(addr.isNull())
		return true;
	
	int error;
	
	if(addr.protocol() == QAbstractSocket::IPv4Protocol)
	{
		sockaddr_in in;
		
		memset(&in, 0, sizeof(in));
		in.sin_addr.s_addr = addr.toIPv4Address();
		in.sin_family = AF_INET;
		
		error = ::bind(sock->socketDescriptor(), (sockaddr*) &in, sizeof(in));
	}
	else
	{
		sockaddr_in6 in6;
		
		Q_IPV6ADDR a = addr.toIPv6Address();
		memset(&in6, 0, sizeof(in6));
		memcpy(in6.sin6_addr.s6_addr, a.c, 16);
		in6.sin6_family = AF_INET6;
		
		error = ::bind(sock->socketDescriptor(), (sockaddr*) &in6, sizeof(in6));
	}
	
	return !error;
}

bool LimitedSocket::request(QString file, bool bWrite)
{
	QIODevice::OpenMode mode;
	
	mode = (bWrite) ? QIODevice::Append : QIODevice::ReadOnly;
	
	m_file.setFileName(file);
	if(!m_file.open(mode))
	{
		qDebug() << "Failed to open file" << file;
		m_strError = tr("Failed to open file");
		return false;
	}
	return true;
}

void LimitedSocket::destroy()
{
	cout << "LimitedSocket::destroy()\n";
	m_bAbort = true;
	
	if(isRunning())
		connect(this, SIGNAL(finished()), this, SLOT(deleteLater()));
	else
		delete this;
}

QByteArray LimitedSocket::readLine()
{
	while(true)
	{
		//cout << "ReadLine cycle\n";
		QByteArray array = m_pSocket->readLine();
		if(!array.isEmpty())
			return array;
		else
		{
			if(!m_pSocket->waitForReadyRead())
				return QByteArray();
		}
	}
}

bool LimitedSocket::writeCycle()
{
	int toRead;
	QByteArray buffer;
	
	if(m_timer.isNull())
	{
		m_prevBytes = 0;
		m_timer.start();
	}
	
	toRead = qMin<qint64>(1024, m_nToTransfer-m_nTransfered);
	buffer = m_file.read(toRead);
	
	//qDebug() << "Reading" << toRead << "bytes";
	
	if(buffer.size() < toRead)
	{
		m_strError = tr("Error reading file");
		return false;
	}
	
	m_pSocket->write(buffer);
	//qDebug() << "Written data";
	if(!m_pSocket->waitForBytesWritten(10000))
	{
		m_strError = tr("Timeout");
		return false;
	}
	
	m_nTransfered += toRead;
	
	QTime now = QTime::currentTime();
	int msecs;
	qint64 bytes = m_nTransfered - m_prevBytes;
	
	if((msecs = m_timer.elapsed()) > 1000)
	{
		m_statsLock.lockForWrite();
		m_stats << QPair<int,qulonglong>(m_timer.restart(), bytes);
		if(m_stats.size() > SPEED_SAMPLES)
			m_stats.removeFirst();
		m_statsLock.unlock();
		m_prevBytes = m_nTransfered;
	}
	
	if(m_nSpeedLimit > 0 && bytes)
	{
		int sleeptime = bytes/(m_nSpeedLimit/1000) - msecs;
		if(sleeptime > 0)
			msleep(sleeptime*2);
	}
	
	return true;
}

bool LimitedSocket::readCycle()
{
	QByteArray buffer;
	bool bProblem = false;
	
	if(m_timer.isNull())
	{
		m_prevBytes = 0;
		m_timer.start();
	}
	
	while(buffer.size() < 1024)
	{
		if(m_nTransfered+buffer.size() >= m_nToTransfer && m_nToTransfer)
			break;
		
		if(!m_pSocket->bytesAvailable() && !m_pSocket->waitForReadyRead())
		{
			bProblem = true;
			break;
		}
		
		qint64 toread = 1024;
		
		if(m_nToTransfer && m_nToTransfer-m_nTransfered < 1024)
			toread = m_nToTransfer-m_nTransfered;
		QByteArray buf = m_pSocket->read(toread);
		
		buffer += buf;
		m_nTransfered += buf.size();
	}
	
	m_file.write(buffer);
	
	if(bProblem)
	{
		if(m_pSocket->state() == QAbstractSocket::ConnectedState) // timeout
			m_strError = tr("Timeout");
		else if(m_pSocket->error() == QAbstractSocket::RemoteHostClosedError)
		{
			if(m_nToTransfer > m_nTransfered && m_nToTransfer)
			{
				qDebug() << "Remote host closed the connection";
				m_strError = tr("Connection lost");
			}
		}
		else
		{
			m_strError = m_pSocket->errorString();
			if(m_strError.isEmpty())
			{
				qDebug() << "Connection lost!";
				m_strError = tr("Connection lost");
			}
		}
	}
	else if(!m_bAbort)
	{
		QTime now = QTime::currentTime();
		qulonglong bnow = m_nTransfered;
	
		int msecs;
		qulonglong bytes = bnow - m_prevBytes;
		
		if((msecs = m_timer.elapsed()) > 1000)
		{
			m_statsLock.lockForWrite();
			m_stats << QPair<int,qulonglong>(m_timer.restart(), bytes);
			if(m_stats.size() > SPEED_SAMPLES)
				m_stats.removeFirst();
			m_statsLock.unlock();
			
			m_prevBytes = bnow;
		}
		
		if(m_nSpeedLimit > 0 && bytes)
		{
			int sleeptime = bytes/(m_nSpeedLimit/1000) - msecs;
			if(sleeptime > 0)
				msleep(sleeptime*2);
		}
	}
	
	return !bProblem;
}

int LimitedSocket::speed() const
{
	int msecs = 0;
	qulonglong bytes = 0;
	
	m_statsLock.lockForRead();
	for(int i=0;i<m_stats.size();i++)
	{
		msecs += m_stats[i].first;
		bytes += m_stats[i].second;
	}
	m_statsLock.unlock();
	
	if(!msecs)
		return 0;
	else
		return 1000*bytes/msecs;
}

void LimitedSocket::reset()
{
	terminate();
	delete m_pSocket;
	m_pSocket = 0;
	m_nTransfered = m_nToTransfer = 0;
	m_stats.clear();
	m_timer = QTime();
	m_file.close();
}

FtpEngine::FtpEngine(QUrl url, QUuid proxyUuid) : m_pSocketMain(0)
{
	m_strUser = url.userName();
	m_strPassword = url.password();
	
	if(m_strUser.isEmpty())
	{
		m_strUser = "anonymous";
		m_strPassword = "fatrat@fatrat.dolezel.info";
	}
	
	m_url = url;
	//m_strHost = url.host();
	//m_nPort = url.port(21);
	//m_strFile = url.path();
	
	QList<Proxy> listProxy = Proxy::loadProxys();
	m_proxyData.nType = Proxy::ProxyNone;
	
	foreach(Proxy p,listProxy)
	{
		if(p.uuid == proxyUuid)
		{
			m_proxyData = p;
			break;
		}
	}
}

void FtpEngine::request(QString file, int flags)
{
	if(LimitedSocket::request(file, flags & FtpGet))
	{
		m_flags = flags;
		
		if(m_flags & FtpGet)
		{
			QString path = m_url.path();
			int pos = path.lastIndexOf('/');
			
			if(pos != -1)
				m_strName = path.mid(pos+1);
			else
				m_strName = path;
			
			m_nResume = m_file.pos();
		}
		else
		{
			QFileInfo finfo(file);
			if(m_strName.isEmpty())
				m_strName = finfo.fileName();
			m_nToTransfer = finfo.size();
		}
		start();
	}
	else
		emit finished(this,true);
}

bool FtpEngine::passiveConnect()
{
	QHostAddress addr;
	quint16 port;
	QString reply;
	
	writeLine("PASV\r\n");
	if(readStatus(reply) != 227)
		return false;
	
	int p,e;
	QStringList ipport;
	
	if((p = reply.indexOf('(') + 1) == 0)
		throw tr("Invalid server response");
	if((e = reply.indexOf(')')) == -1)
		throw tr("Invalid server response");
	
	ipport = reply.mid(p,e-p).split(',');
	if(ipport.size() != 6)
		throw tr("Invalid server response");
	
	addr = QHostAddress(qntoh(ipport[0].toUInt() | ipport[1].toUInt()<<8 | ipport[2].toUInt()<<16 | ipport[3].toUInt()<<24));
	port = ipport[4].toUShort()*0x0100 | ipport[5].toUShort();
	
	m_pSocket = new QTcpSocket;
	m_pSocket->connectToHost(addr,port);
	
	qDebug() << "Connecting to" << addr << "port" << port;
	
	QThread::msleep(500); // sometimes doesn't work without this sleep
	if(!m_pSocket->waitForConnected())
	{
		doClose(m_pSocket);
		return false;
	}
	else
		return true;
}

bool FtpEngine::activeConnect(QTcpServer& server)
{
	QString ipport,reply;
	QHostAddress local = m_pSocketMain->localAddress();
	quint32 ip = local.toIPv4Address();
	quint16 port;
	
	while(true) // FIXME
	{
		port = ++m_ftpPort;
		if(server.listen(local, port))
			break;
	}
	
	server.setMaxPendingConnections(1);
	
	writeLine(QString("PORT %1,%2,%3,%4,%5,%6\r\n").arg(ip>>24).arg((ip>>16)&0xff).arg((ip>>8)&0xff).arg(ip&0xff).arg(port>>8).arg(port&0xff));
	
	if(readStatus(reply) != 200)
		return false;
	
	return true;
}

bool FtpEngine::activeConnectFin(QTcpServer& server)
{
	if(!server.waitForNewConnection(10000))
		return false;
	m_pSocket = server.nextPendingConnection();
	return true;
}

void FtpEngine::login()
{
	int code;
	QString reply;
	
	emit status(tr("Logging in"));
		
	writeLine(QString("USER %1\r\n").arg(m_strUser));
	code = readStatus(reply);
	if(code != 331 && code != 230)
		throw reply;
	
	if(code != 230)
	{
		writeLine(QString("PASS %1\r\n").arg(m_strPassword));
		if(readStatus(reply) != 230)
			throw reply;
	}
}

void FtpEngine::connectServer()
{
	QString reply;
	
	if(m_pSocketMain)
		delete m_pSocketMain;
	
	m_pSocketMain = new QTcpSocket;
		
	if(m_proxyData.nType == Proxy::ProxySocks5)
	{
		QNetworkProxy proxy;
		
		proxy.setType(QNetworkProxy::Socks5Proxy);
		proxy.setHostName(m_proxyData.strIP);
		proxy.setPort(m_proxyData.nPort);
		proxy.setUser(m_proxyData.strUser);
		proxy.setPassword(m_proxyData.strPassword);
		
		m_pSocketMain->setProxy(proxy);
	}
	
	m_pSocketMain->connectToHost(m_url.host(), m_url.port(21));
	
	emit status("Connecting");
	if(!m_pSocketMain->waitForConnected())
	{
		qDebug() << "Failed to connect" << m_pSocketMain->errorString();
		throw m_pSocketMain->errorString();
	}
	
	if(readStatus(reply) != 220)
		throw reply;
}

qint64 FtpEngine::querySize()
{
	QString reply;
	qulonglong size;
	
	emit status(tr("Querying file size"));
	
	writeLine(QString("SIZE %1\r\n").arg(m_strName));
	if(readStatus(reply) != 213)
		return 0;
	size = reply.mid(4).trimmed().toULongLong();
	
	emit responseReceived(size);
	return size;
}

void FtpEngine::switchToDirectory()
{
	QString reply, dir;
	
	dir = m_url.path();
	
	if(m_flags & FtpGet)
	{
		int pos = dir.lastIndexOf('/');
		
		if(pos != -1)
			dir = dir.left(pos);
	}
	
	if(dir.isEmpty())
		dir = "/";
	
	emit status(tr("Switching directory"));
		writeLine(QString("CWD %1\r\n").arg(dir));
	if(readStatus(reply) != 250)
		throw reply;
}

void FtpEngine::requestFile()
{
	QString reply;
	emit status(tr("Requesting file"));
	
	writeLine(QString("RETR %1\r\n").arg(m_strName));
	
	if(readStatus(reply) != 150)
		throw reply;
	/*else
	{
		QRegExp rx("\\((\\d+)");
		int pos = rx.indexIn(reply);
		
		if(pos < 0)
		{
			qDebug() << "Failed to get file size" << reply;
			return 0;
		}
		else
		{
			qulonglong bytes = m_nResume + rx.capturedTexts()[1].toULongLong();
			emit responseReceived(bytes);
			return bytes;
		}
	}*/
	qDebug() << "RequestFile end";
}

void FtpEngine::appendFile()
{
	QString reply;
	emit status(tr("Appending file"));
	
	writeLine(QString("APPE %1\r\n").arg(m_strName));
	
	if(readStatus(reply) != 150)
		throw reply;
}

void FtpEngine::switchToBinary()
{
	QString reply;
	writeLine("TYPE I\r\n");
	if(readStatus(reply) != 200)
		throw reply;
}

void FtpEngine::setResume()
{
	if(!m_nResume)
		return;
	
	QString reply;
	writeLine(QString("REST %1\r\n").arg(m_nResume));
	if(readStatus(reply) != 350)
		throw reply;
}

void FtpEngine::run()
{
	QTcpServer server;
	try
	{
		QString reply,cmd;
		bool switchedMode = false, passive = true;
		
		if(m_proxyData.nType == Proxy::ProxyNone)
			passive = m_flags & FtpPassive;
		
		connectServer();
		login();
		switchToBinary();
		switchToDirectory();

econn: // FIXME: this all looks so wrong
		emit status(tr("Establishing file connection"));
		while(true)
		{
			bool bError = false;
			if(passive)
			{
				if(!passiveConnect())
					bError = true;
			}
			else if(!activeConnect(server))
				bError = true;
			
			if(bError)
			{
				if(switchedMode)
					throw tr("Unable to establish the data connection");
				else if(m_proxyData.nType == Proxy::ProxyNone)
				{
					switchedMode = true;
					passive = !passive;
				}
			}
			else
				break;
		}
		
		qint64 size = querySize();
		
		if(m_flags & FtpGet)
		{
			if(size > 0)
				m_nToTransfer = size - m_nResume;
			else
				m_nToTransfer = 0;
			setResume();
			requestFile();
		}
		else
		{
			m_nResume = size;
			appendFile();
			m_nToTransfer -= m_nResume;
		}
		
		if(!passive && !activeConnectFin(server))
		{
			if(!switchedMode)
				goto econn;
			else
				throw tr("Unable to establish the data connection");
		}
		
		m_pSocket->setReadBufferSize(1024);
		
		while(!m_bAbort && m_nTransfered < m_nToTransfer)
		{
			bool success;
			
			if(m_flags & FtpGet)
				success = readCycle();
			else
				success = writeCycle();
			
			if(!success)
				break;
		}
		
		//if(m_bAbort)
		//	qDebug() << "Aborted";
		//else if(m_nTransfered >= m_nToTransfer)
		//	qDebug() << "Was done";
		
		doClose(m_pSocket);
		readStatus(reply);
		
		m_file.close();
		if(!m_bAbort)
			emit finished(this,!m_strError.isEmpty());
		else
			m_bAbort = false;
	}
	catch(QString msg)
	{
		qDebug() << "Exception:" << msg;
		if(!m_bAbort)
		{
			m_strError = msg.trimmed();
			emit finished(this,true);
		}
		else
			m_bAbort = false;
	}
	
	if(m_pSocket)
		doClose(m_pSocket);
	if(m_pSocketMain)
		doClose(m_pSocketMain);
}

void LimitedSocket::doClose(QTcpSocket*& sock)
{
	if(!sock)
		return;
	sock->close();
	
	QAbstractSocket::SocketState state = sock->state();
	if(state == QAbstractSocket::ConnectedState || state == QAbstractSocket::ClosingState)
		sock->waitForDisconnected(5000);
	delete sock;
	sock = 0;
}

void FtpEngine::writeLine(QString line)
{
	qDebug() << "writeLine()" << line;
	
	if(!line.startsWith("PASS"))
		emit logMessage(line.trimmed().prepend("<<< "));
	else
		emit logMessage("<<< PASS *****");
	
	if(m_pSocketMain->write(line.toAscii()) == -1)
		throw m_pSocketMain->errorString();
	if(!m_pSocketMain->waitForBytesWritten())
		throw tr("Timeout");
}

int FtpEngine::readStatus(QString& cline)
{
	while(!m_bAbort)
	{
		QByteArray line;
		line = m_pSocketMain->readLine();
		
		if(line.isEmpty())
		{
			if(!m_pSocketMain->waitForReadyRead())
				throw(tr("Connection to the server lost"));
			else
				continue;
		}
		
		/*if(line.size() < 4)
		{
			qDebug() << "Invalid response:" << line;
			throw(tr("Invalid server response"));
		}*/
		
		if(line[3] == ' ' && isdigit(line[2]))
		{
			qDebug() << "readStatus()" << line;
			cline = line;
			emit logMessage(line.trimmed().prepend(">>> "));
			return line.left(3).toInt();
		}
	}
	
	cline = QString();
	return 0;
}

