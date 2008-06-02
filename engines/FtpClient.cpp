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

#include "FtpClient.h"
#include <QFileInfo>
#include <QStringList>
#include <QNetworkProxy>

Q_GLOBAL_STATIC(ActivePortAllocator, activePortAllocator);

ActivePortAllocator::ActivePortAllocator()
{
	m_nRangeStart = 40000;
}

QTcpServer* ActivePortAllocator::getNextPort()
{
	m_mutex.lock();
	
	QTcpServer* server = 0;
	
	for(unsigned short port = m_nRangeStart; !server; port++)
	{
		if(m_existing.contains(port))
			continue;
		
		server = new QTcpServer;
		if(!server->listen(QHostAddress::Any, port))
		{
			delete server;
			server = 0;
		}
		else
		{
			connect(server, SIGNAL(destroyed(QObject*)), this, SLOT(listenerDestroyed(QObject*)));
			m_existing[port] = server;
		}
	}
	
	m_mutex.unlock();
	
	return server;
}

void ActivePortAllocator::listenerDestroyed(QObject* obj)
{
	m_mutex.lock();
	
	unsigned short port = m_existing.key((QTcpServer*)obj);
	m_existing.remove(port);
	
	m_mutex.unlock();
}

FtpEngine::FtpEngine(QUrl url, QUuid proxyUuid) : m_pRemote(0), m_pSocketMain(0)
{
	m_strUser = url.userName();
	m_strPassword = url.password();
	
	if(m_strUser.isEmpty())
	{
		m_strUser = "anonymous";
		m_strPassword = "fatrat@ihatepasswords.ww";
	}
	
	m_url = url;
	
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

void FtpEngine::request(QString file, bool bUpload, int flags)
{
	if(LimitedSocket::open(file, bUpload))
	{
		m_flags = flags;
		
		if(!bUpload)
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
	{
		m_strError = tr("Failed to open file");
		emit finished(true);
	}
}

bool FtpEngine::passiveConnect()
{
	QHostAddress addr;
	quint16 port;
	QString reply;
	
	writeLine("EPSV\r\n");
	if(readStatus(reply) != 229) // 227 = PORT
		return false;
		
	/*int p,e;
	QStringList ipport;
	
	if((p = reply.indexOf('(') + 1) == 0)
		throw tr("Invalid server response");
	if((e = reply.indexOf(')')) == -1)
		throw tr("Invalid server response");
	
	ipport = reply.mid(p,e-p).split(',');
	if(ipport.size() != 6)
		throw tr("Invalid server response");
	
	addr = QHostAddress(qntoh(ipport[0].toUInt() | ipport[1].toUInt()<<8 | ipport[2].toUInt()<<16 | ipport[3].toUInt()<<24));
	port = ipport[4].toUShort()*0x0100 | ipport[5].toUShort();*/
	
	QRegExp re("\\|(\\d*)\\|([^\\|]*)\\|(\\d+)\\|");
	if(re.indexIn(reply) < 0)
		throw tr("Invalid server response");
	
	port = re.cap(3).toUShort();
	if(!re.cap(2).isEmpty())
		addr = re.cap(2);
	else
		addr = m_pSocketMain->peerAddress();
	
	m_pRemote = new QTcpSocket;
	m_pRemote->connectToHost(addr,port);
	
	qDebug() << "Connecting to" << addr << "port" << port;
	
	QThread::msleep(500); // sometimes doesn't work without this sleep
	if(!m_pRemote->waitForConnected())
	{
		doClose(&m_pRemote);
		return false;
	}
	else
		return true;
}

bool FtpEngine::activeConnect(QTcpServer** server)
{
	QString ipport,reply;
	QHostAddress local = m_pSocketMain->localAddress();
	quint16 port;
	
	QTcpServer* srv;
	
	srv = *server = activePortAllocator()->getNextPort();
	
	srv->setMaxPendingConnections(1);
	port = srv->serverPort();
	
	//writeLine(QString("PORT %1,%2,%3,%4,%5,%6\r\n").arg(ip>>24).arg((ip>>16)&0xff).arg((ip>>8)&0xff).arg(ip&0xff).arg(port>>8).arg(port&0xff));
	QString line;
	line = QString("EPRT |%1|%2|%3|\n").arg(local.protocol() == QAbstractSocket::IPv4Protocol ? 1 : 2).arg(local.toString()).arg(port);
	//line = QString("EPRT |||%1|\n").arg(port);
	writeLine(line);
	
	if(readStatus(reply) != 200)
		return false;
	
	return true;
}

bool FtpEngine::activeConnectFin(QTcpServer* server)
{
	if(!server->waitForNewConnection(10000))
		return false;
	m_pRemote = server->nextPendingConnection();
	return true;
}

void FtpEngine::login()
{
	int code;
	QString reply;
	
	emit statusMessage(tr("Logging in"));
		
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
	
	/*m_pSocketMain->connectToHost(m_url.host(), m_url.port(21));
	
	emit statusMessage("Connecting");
	if(!m_pSocketMain->waitForConnected())
	{
		qDebug() << "Failed to connect" << m_pSocketMain->errorString();
		throw m_pSocketMain->errorString();
	}*/
	
	connectToHost(m_pSocketMain, m_url.host(), m_url.port(21));
	
	if(readStatus(reply) != 220)
		throw reply;
}

qint64 FtpEngine::querySize()
{
	QString reply;
	qulonglong size;
	
	emit statusMessage(tr("Querying file size"));
	
	writeLine(QString("SIZE %1\r\n").arg(m_strName));
	if(readStatus(reply) != 213)
		return 0;
	size = reply.mid(4).trimmed().toULongLong();
	
	emit receivedSize(size);
	return size;
}

void FtpEngine::switchToDirectory()
{
	QString reply, dir;
	
	dir = m_url.path();
	
	if(!m_bUpload)
	{
		int pos = dir.lastIndexOf('/');
		
		if(pos != -1)
			dir = dir.left(pos);
	}
	
	if(dir.isEmpty())
		dir = "/";
	
	emit statusMessage(tr("Switching directory"));
		writeLine(QString("CWD %1\r\n").arg(dir));
	if(readStatus(reply) != 250)
		throw reply;
}

void FtpEngine::requestFile()
{
	QString reply;
	emit statusMessage(tr("Requesting file"));
	
	writeLine(QString("RETR %1\r\n").arg(m_strName));
	
	if(readStatus(reply) != 150)
		throw reply;
	
	qDebug() << "RequestFile end";
}

void FtpEngine::appendFile()
{
	QString reply;
	emit statusMessage(tr("Appending file"));
	
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
	QTcpServer* server = 0;
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
		emit statusMessage(tr("Establishing a data connection"));
		while(true)
		{
			bool bError = false;
			if(passive)
			{
				if(!passiveConnect())
					bError = true;
			}
			else if(!activeConnect(&server))
				bError = true;
			
			if(bError)
			{
				if(switchedMode)
					throw tr("Unable to establish a data connection");
				else if(m_proxyData.nType == Proxy::ProxyNone)
				{
					switchedMode = true;
					passive = !passive;
				}
			}
			else
				break;
		}
		
		if(!m_bUpload)
		{
			if(m_nSegmentEnd < 0)
			{
				qint64 size = querySize();
				if(size > 0)
					m_nToTransfer = size - m_nResume;
				else
					m_nToTransfer = 0;
			}
			else
				m_nToTransfer = m_nSegmentEnd - m_nResume;
			
			setResume();
			requestFile();
		}
		else
		{
			m_nResume = querySize();
			appendFile();
			m_nToTransfer -= m_nResume;
		}
		
		if(!passive && !activeConnectFin(server))
		{
			if(!switchedMode)
				goto econn;
			else
				throw tr("Unable to establish a data connection");
		}
		
		while(!m_bAbort && m_nTransfered < m_nToTransfer)
		{
			bool success;
			
			if(!m_bUpload)
				success = readCycle();
			else
				success = writeCycle();
			
			if(!success)
				throw getErrorString(m_pRemote->error());
		}
		
		doClose(&m_pRemote);
		readStatus(reply);
		
		m_file.close();
		if(!m_bAbort)
			emit finished(false);
	}
	catch(QString msg)
	{
		qDebug() << "Exception:" << msg;
		if(!m_bAbort)
		{
			m_strError = msg.trimmed();
			emit finished(true);
		}
	}
	
	server->deleteLater();
	
	if(m_pRemote)
		doClose(&m_pRemote);
	if(m_pSocketMain)
		doClose(&m_pSocketMain);
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

