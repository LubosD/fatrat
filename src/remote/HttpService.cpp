/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2009 Lubos Dolezel <lubos a dolezel.info>

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

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#include "config.h"

#include "HttpService.h"
#include "Settings.h"
#include "RuntimeException.h"
#include "SettingsWebForm.h"
#include "SpeedGraph.h"
#include "Transfer.h"
#include "Queue.h"
#include "fatrat.h"
#include "Logger.h"
#include "dbus/DbusImpl.h"
#include "poller/Poller.h"
#include "remote/XmlRpcService.h"

#include <QSettings>
#include <QtDebug>
#include <QStringList>
#include <QFile>
#include <QUrl>
#include <QDir>
#include <QTcpSocket>
#include <QBuffer>
#include <QImage>
#include <QPainter>
#include <QFileInfo>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdlib>
#include <locale.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;
extern QSettings* g_settings;
extern QVector<EngineEntry> g_enginesDownload;

static const int SOCKET_TIMEOUT = 10;

HttpService* HttpService::m_instance = 0;

#define HTTP_HEADERS "Server: FatRat/" VERSION "\r\n"

struct AuthenticationFailure {};

HttpService::HttpService()
{
	m_instance = this;
	applySettings();
	
	SettingsItem si;
	si.lpfnCreate = SettingsWebForm::create;
	si.title = tr("Web Interface");
	si.icon = DelayedIcon(":/fatrat/webinterface.png");
	
	addSettingsPage(si);
}

HttpService::~HttpService()
{
	m_instance = 0;
	if(isRunning())
	{
		m_bAbort = true;
		wait();
		close(m_server);
	}
}

void HttpService::applySettings()
{
	bool enable = getSettingsValue("remote/enable").toBool();
	if(enable && !isRunning())
	{
		try
		{
			m_bAbort = false;
			setup();
			start();
		}
		catch(const RuntimeException& e)
		{
			qDebug() << e.what();
			Logger::global()->enterLogMessage("HttpService", e.what());
		}
	}
	else if(!enable && isRunning())
	{
		m_bAbort = true;
		wait();
		close(m_server);
		m_server = 0;
	}
}

void HttpService::setup()
{
	quint16 port;
	bool bIPv6 = true;
	
	port = getSettingsValue("remote/port").toUInt();
	
	if((m_server = socket(PF_INET6, SOCK_STREAM, 0)) < 0)
	{
		if(errno == EAFNOSUPPORT)
		{
			bIPv6 = false;
			
			if((m_server = socket(PF_INET, SOCK_STREAM, 0)) < 0)
				throwErrno();
		}
		else
			throwErrno();
	}
	
	int reuse = 1;
	setsockopt(m_server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	
	if(bIPv6)
	{
		sockaddr_in6 addr6;
		memset(&addr6, 0, sizeof addr6);
		addr6.sin6_family = AF_INET6;
		addr6.sin6_port = htons(port);
		addr6.sin6_addr = in6addr_any;
		
		if(bind(m_server, (sockaddr*) &addr6, sizeof addr6) < 0)
			throwErrno();
	}
	else
	{
		sockaddr_in addr4;
		addr4.sin_family = AF_INET;
		addr4.sin_port = htons(port);
		addr4.sin_addr.s_addr = INADDR_ANY;
		
		if(bind(m_server, (sockaddr*) &addr4, sizeof addr4) < 0)
			throwErrno();
	}
	
	if(listen(m_server, 10) < 0)
		throwErrno();
	
	Logger::global()->enterLogMessage("HttpService", tr("Listening on port %1").arg(port));
}

void HttpService::throwErrno()
{
	QString err = QString::fromUtf8(strerror(errno));
	throw RuntimeException(err);
}

void HttpService::run()
{
	Poller* poller = Poller::createInstance();
	
	poller->addSocket(m_server, Poller::PollerIn | Poller::PollerOut | Poller::PollerError | Poller::PollerHup);
	
	while(!m_bAbort)
	{
		Poller::Event events[30];
		int nev = poller->wait(500, events, sizeof(events)/sizeof(events[0]));
		
		for(int i=0;i<nev;i++)
		{
			const Poller::Event& ev = events[i];
			
			if(ev.socket == m_server)
			{
				sockaddr addr;
				socklen_t len = sizeof addr;
				int client = accept(m_server, &addr, &len);
				
				if(client < 0)
					continue;
				
				int arg = fcntl(client, F_GETFL);
				fcntl(client, F_SETFL, arg | O_NONBLOCK);
				
				poller->addSocket(client, Poller::PollerIn | Poller::PollerOut | Poller::PollerError | Poller::PollerHup);
			}
			else
			{
				bool bTerminate = false;
				
				if(ev.flags & (Poller::PollerError | Poller::PollerHup))
					bTerminate = true;
				else if(ev.flags & Poller::PollerIn && !processClientRead(ev.socket))
					bTerminate = true;
				else if(ev.flags & Poller::PollerOut && !processClientWrite(ev.socket))
					bTerminate = true;
				
				if(bTerminate)
				{
					freeClient(ev.socket, poller);
					m_clients.remove(ev.socket);
				}
			}
		}
		
		for(QMap<int, ClientData>::iterator it = m_clients.begin(); it != m_clients.end();)
		{
			if(time(0) - it.value().lastData > SOCKET_TIMEOUT)
			{
				freeClient(it.key(), poller);
				it = m_clients.erase(it);
			}
			else
				it++;
		}
	}
	
	for(QMap<int, ClientData>::iterator it = m_clients.begin(); it != m_clients.end();)
	{
		freeClient(it.key(), poller);
		it = m_clients.erase(it);
	}
	
	delete poller;
}

void HttpService::freeClient(int fd, Poller* poller)
{
	ClientData& data = m_clients[fd];
	delete data.file;
	delete data.buffer;
	poller->removeSocket(fd);
	close(fd);
}

long HttpService::contentLength(const QList<QByteArray>& data)
{
	for(int i=0;i<data.size();i++)
	{
		if(!strncasecmp(data[i].constData(), "Content-Length: ", 16))
			return atol(data[i].constData()+16);
	}
	return 0;
}

bool HttpService::processClientRead(int fd)
{
	char buffer[4096];
	ClientData& data = m_clients[fd];
	
	while(true)
	{
		ssize_t bytes = read(fd, buffer, sizeof buffer);
		
		if(bytes < 0)
		{
			if(errno == EAGAIN)
				break;
			else
				return false;
		}
		else if(!bytes)
			return false;
		
		char* start = buffer;
		time(&data.lastData);
		
		// parse lines
		while(bytes)
		{
			// processing POST data
			if(!data.requests.isEmpty() && data.requests.back().contentLength)
			{
				HttpRequest& rq = data.requests.back();
				long cp = qMin<long>(rq.contentLength, bytes);
				
				rq.postData += QByteArray(start, cp);
				rq.contentLength -= cp;
				
				bytes -= cp;
				start += cp;
			}
			else // processing HTTP headers
			{
				if(data.incoming.isEmpty())
					data.incoming << QByteArray();
				
				char* p = (char*) memchr(start, '\n', bytes);
				
				if(!p)
				{
					data.incoming.back() += QByteArray(start, bytes);
					bytes = 0;
				}
				else
				{
					ssize_t now = p-start+1;
					
					data.incoming.back() += QByteArray(start, p-start+1);
					bytes -= now;
					start = p + 1;
					
					if(data.incoming.back() == "\r\n") // end of the request
					{
						HttpRequest rq;
						
						rq.lines = data.incoming;
						rq.contentLength = contentLength(rq.lines);
						
						data.incoming.clear();
						data.requests << rq;
					}
					else
						data.incoming << QByteArray();
				}
			}
		}
	}
	
	for(int i=0;i<data.requests.size();)
	{
		if(!data.requests[i].contentLength)
		{
			serveClient(fd);
			data.requests.removeAt(i);
		}
		else
			break;
	}
	
	return true;
}

bool HttpService::processClientWrite(int fd)
{
	ClientData& data = m_clients[fd];
	time(&data.lastData);
	
	if(data.file)
	{
		if(!data.file->atEnd() && data.file->isOpen())
		{
			QByteArray buf = data.file->read(2*1024);
			
			return write(fd, buf.constData(), buf.size()) == buf.size();
		}
	}
	else if(data.buffer && !data.buffer->isEmpty())
	{
		QByteArray ar;
		unsigned long bytes = 8*1024;
		
		ar.resize(bytes);
		data.buffer->getData(ar.data(), &bytes);
		
		int written = write(fd, ar.constData(), bytes);
		if(written <= 0)
			return false;
		else
			data.buffer->removeData(written);
	}
	
	return true;
}

bool HttpService::authenticate(const QList<QByteArray>& data)
{
	for(int i=0;i<data.size();i++)
	{
		if(!strncasecmp(data[i].constData(), "Authorization: Basic ", 21))
		{
			QList<QByteArray> auth = QByteArray::fromBase64(data[i].mid(21)).split(':');
			if(auth.size() != 2)
				return false;
			
			return auth[1] == getSettingsValue("remote/password").toString().toUtf8();
		}
	}
	return false;
}


QByteArray HttpService::graph(QString queryString)
{
	QBuffer buf;
	QImage image(QSize(640, 480), QImage::Format_RGB32);
	
	Queue* q;
	Transfer* t;
	
	findTransfer(queryString, &q, &t);
	if(!q || !t)
		return QByteArray();
	
	SpeedGraph::draw(t->speedData(), QSize(640, 480), &image);
	
	q->unlock();
	g_queuesLock.unlock();
	
	image.save(&buf, "PNG");
	return buf.data();
}

QByteArray HttpService::qgraph(QString queryString)
{
	QBuffer buf;
	QImage image(QSize(640, 480), QImage::Format_RGB32);

	QReadLocker locker(&g_queuesLock);
	Queue* q;

	findQueue(queryString, &q);
	if(!q)
		return QByteArray();

	SpeedGraph::draw(q->speedData(), QSize(640, 480), &image);

	image.save(&buf, "PNG");
	return buf.data();
}

QByteArray HttpService::copyrights()
{
	const char* loc = DATA_LOCATION "/README";
	QFile file;
	file.setFileName(loc);
	file.open(QIODevice::ReadOnly);
	return file.readAll();
}

void HttpService::serveClient(int fd)
{
	char buffer[4096];
	bool bHead, bGet, bPost;
	qint64 fileSize = -1;
	QDateTime modTime;
	QByteArray fileName, queryString, postData, disposition;
	ClientData& data = m_clients[fd];
	HttpRequest& rq = data.requests[0];
	QList<QByteArray> firstLine = rq.lines[0].split(' ');
	
	bHead = firstLine[0] == "HEAD";
	bGet = firstLine[0] == "GET";
	bPost = firstLine[0] == "POST";
	
	qDebug() << postData;
	if(bGet || bHead || bPost)
	{
		if(firstLine.size() >= 2)
			fileName = firstLine[1];
		if(bPost)
			postData = rq.postData;
	}
	
	int q = fileName.indexOf('?');
	if(q != -1)
	{
		queryString = fileName.mid(q+1);
		fileName.resize(q);
	}
	
	try
	{
		if(!authenticate(rq.lines))
			throw AuthenticationFailure();

		if(fileName.isEmpty() || fileName.indexOf("/..") != -1 || fileName.indexOf("../") != -1)
		{
			const char* msg = "HTTP/1.0 400 Bad Request\r\n" HTTP_HEADERS "\r\n";
			(void) write(fd, msg, strlen(msg));
			return;
		}
		
		if(fileName == "/")
			fileName = "/index.html";
		
		if(fileName.startsWith("/generate/"))
		{
			if(!bHead)
			{
				QByteArray png;
				QString q;
				int index;

				data.buffer = new OutputBuffer;

				q = urlDecode(queryString);

				if ((index = q.indexOf('&')) != -1)
					q = q.left(index);
				
				if(fileName.endsWith("/graph.png"))
					png = graph(q);
				else if(fileName.endsWith("/qgraph.png"))
					png = qgraph(q);
				else
				{
					delete data.buffer;
					data.buffer = 0;
				}
				
				if(data.buffer)
				{
					qDebug() << "Storing" << png.size() << "bytes";
					data.buffer->putData(png.constData(), png.size());
					fileSize = data.buffer->size();
				}
			}
		}
		else if(fileName == "/copyrights")
		{
			QByteArray file = copyrights();
			data.buffer = new OutputBuffer;
			data.buffer->putData(file.constData(), file.size());
			fileSize = data.buffer->size();
		}
		else if(fileName.startsWith("/log"))
		{
			int pos = fileName.indexOf('/', 1);
			QString uuid;

			if (pos != -1)
				uuid = urlDecode(fileName.mid(pos+1));

			QByteArray file = copyLog(uuid);
			data.buffer = new OutputBuffer;
			data.buffer->putData(file.constData(), file.size());

			sprintf(buffer, "HTTP/1.0 200 OK\r\n" HTTP_HEADERS
					"Content-Type: text/plain; encoding=utf-8\r\n"
					"Cache-Control: no-cache\r\nPragma: no-cache\r\n"
					"Content-Length: %d\r\n\r\n", data.buffer->size());
		}
		else if(fileName == "/download")
		{
			QMap<QString,QString> gets = processQueryString(queryString);
			QString t;
			QString path;
			Queue* qo = 0;
			Transfer* to = 0;
			
			try
			{
				t = gets["transfer"];
				path = gets["path"];
				
				if(path.indexOf("/..") != -1 || path.indexOf("../") != -1)
					throw 0;
				
				findTransfer(t, &qo, &to);
				if(!qo || !to)
					throw 0;
				
				path.prepend(to->dataPath(true));
				
				QFileInfo info(path);
				if(!info.exists())
					throw 0;
				
				modTime = info.lastModified();
				fileSize = info.size();
			}
			catch(...)
			{
				path.clear();
			}
			
			if(qo && to)
			{
				qo->unlock();
				g_queuesLock.unlock();
			}
			
			if(!path.isEmpty() && !bHead)
			{
				int last = path.lastIndexOf('/');
				if(last != -1)
				{
					disposition = path.mid(last+1).toUtf8();
					if(disposition.size() > 100)
						disposition.resize(100);
				}
				
				data.file = new QFile(path);
				data.file->open(QIODevice::ReadOnly);
			}
		}
		else if(fileName == "/xmlrpc")
		{
			if(bPost)
			{
				data.buffer = new OutputBuffer;
				XmlRpcService::serve(postData, data.buffer);

				sprintf(buffer, "HTTP/1.0 200 OK\r\n" HTTP_HEADERS
						"Content-Type: text/xml; encoding=utf-8\r\n"
						"Cache-Control: no-cache\r\nPragma: no-cache\r\n"
						"Content-Length: %d\r\n\r\n", data.buffer->size());
			}
			else
				throw "405 Method Not Allowed";
		}
		else
		{
			fileName.prepend(DATA_LOCATION "/data/remote");
			qDebug() << "Opening" << fileName;
			
			QFileInfo info(fileName);
			if(info.exists())
			{
				if(info.isDir())
					throw "403 Forbidden";
				
				modTime = info.lastModified();
				fileSize = info.size();
				
				if(!bHead)
				{
					data.file = new QFile(fileName);
					data.file->open(QIODevice::ReadOnly);
				}
			}
			else
				throw "404 Not Found";
		}
		
		if(bHead && fileSize < 0)
		{
			strcpy(buffer, "HTTP/1.0 200 OK\r\n" HTTP_HEADERS
					"Cache-Control: no-cache\r\nPragma: no-cache\r\n\r\n");
		}
		else if(fileSize != -1)
		{
			sprintf(buffer, "HTTP/1.0 200 OK\r\n" HTTP_HEADERS "Content-Length: %lld\r\n", fileSize);
			
			if(!modTime.isNull())
			{
				char time[100];
				timet2lastModified(modTime.toTime_t(), time, sizeof time);
				strcat(buffer, time);
			}
			else
			{
				strcat(buffer, "Cache-Control: no-cache\r\nPragma: no-cache\r\n");
			}
			
			if(!disposition.isEmpty())
			{
				char disp[200];
				sprintf(disp, "Content-Disposition: attachment; filename=\"%s\"\r\n", disposition.constData());
				strcat(buffer, disp);
			}
			
			strcat(buffer, "\r\n");
		}
	}
	catch(const char* errorMsg)
	{
		sprintf(buffer, "HTTP/1.0 %s\r\nContent-Length: %ld\r\n%s\r\n%s", errorMsg, strlen(errorMsg), HTTP_HEADERS, errorMsg);
	}
	catch(AuthenticationFailure)
	{
		strcpy(buffer, "HTTP/1.0 401 Unauthorized\r\nWWW-Authenticate: Basic realm=\"FatRat web interface\""
			"\r\nContent-Length: 16\r\n" HTTP_HEADERS "\r\n401 Unauthorized");
	}
	
	int l = (int) strlen(buffer);
	if(write(fd, buffer, l) == l)
		processClientWrite(fd);
}

void HttpService::timet2lastModified(time_t t, char* buffer, size_t size)
{
	struct tm tt;
	char locale[20];

	gmtime_r(&t, &tt);
	strcpy(locale, setlocale(LC_TIME, 0));
	setlocale(LC_TIME, "C");

	strftime(buffer, size, "Last-Modified: %a, %d %b %Y %T %Z\r\n", &tt);

	setlocale(LC_TIME, locale);
}

QMap<QString,QString> HttpService::processQueryString(QByteArray queryString)
{
	QMap<QString,QString> result;
	QStringList s = urlDecode(queryString).split('&');
	foreach(QString e, s)
	{
		int p = e.indexOf('=');
		if(p < 0)
			continue;
		result[e.left(p)] = e.mid(p+1);
	}
	return result;
}

QString HttpService::urlDecode(QByteArray arr)
{
	arr.replace('+', ' ');
	return QUrl::fromPercentEncoding(arr);
}

int HttpService::findTransfer(QString transferUUID, Queue** q, Transfer** t, bool lockForWrite)
{
	*q = 0;
	*t = 0;
	
	g_queuesLock.lockForRead();
	for(int i=0;i<g_queues.size();i++)
	{
		Queue* c = g_queues[i];
		if (lockForWrite)
			c->lockW();
		else
			c->lock();
		
		for(int j=0;j<c->size();j++)
		{
			if(c->at(j)->uuid() == transferUUID)
			{
				*q = c;
				*t = c->at(j);
				return j;
			}
		}
		
		c->unlock();
	}

	g_queuesLock.unlock();
	return -1;
}

void HttpService::findQueue(QString queueUUID, Queue** q)
{
	*q = 0;

	QReadLocker r(&g_queuesLock);
	for(int i=0;i<g_queues.size();i++)
	{
		Queue* c = g_queues[i];

		if (c->uuid() == queueUUID)
		{
			*q = c;
			return;
		}
	}
}

QByteArray HttpService::copyLog(QString uuidTransfer)
{
	if (uuidTransfer.isEmpty())
		return Logger::global()->logContents().toUtf8();
	else
	{
		Queue* q = 0;
		Transfer* t = 0;
		findTransfer(uuidTransfer, &q, &t);

		if (!q || !t)
			return QByteArray();

		QByteArray rv = t->logContents().toUtf8();

		q->unlock();
		g_queuesLock.unlock();

		return rv;
	}
}

