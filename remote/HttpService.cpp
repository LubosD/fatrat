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

#include "config.h"

#include "HttpService.h"
#include "Settings.h"
#include "RuntimeException.h"
#include "SettingsWebForm.h"
#include "Queue.h"
#include "fatrat.h"
#include "Logger.h"

#include <QSettings>
#include <QtDebug>
#include <QStringList>
#include <QFile>
#include <QUrl>
#include <QTcpSocket>
#include <QScriptEngine>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;
extern QSettings* g_settings;

static const int SOCKET_TIMEOUT = 10;

HttpService* HttpService::m_instance = 0;

#define HTTP_HEADERS "Server: FatRat/" VERSION "\r\n"

HttpService::HttpService()
{
	m_instance = this;
	applySettings();
	
	SettingsItem si;
	si.lpfnCreate = SettingsWebForm::create;
	si.icon = QIcon(":/fatrat/webinterface.png");
	
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
	int ep;
	epoll_event ev, events[10];
	
	ep = epoll_create(30);
	
	ev.events = EPOLLIN | EPOLLPRI | EPOLLHUP | EPOLLERR;
	ev.data.fd = m_server;
	
	epoll_ctl(ep, EPOLL_CTL_ADD, m_server, &ev);
	
	while(!m_bAbort)
	{
		int nfds = epoll_wait(ep, events, 10, 500);
		
		for(int i=0;i<nfds;i++)
		{
			int fd = events[i].data.fd;
			
			if(fd == m_server)
			{
				sockaddr addr;
				socklen_t len = sizeof addr;
				int client = accept(m_server, &addr, &len);
				
				if(client < 0)
					continue;
				
				int arg = fcntl(client, F_GETFL);
				fcntl(client, F_SETFL, arg | O_NONBLOCK);
				
				ev.events = EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLOUT;
				ev.data.fd = client;
				
				epoll_ctl(ep, EPOLL_CTL_ADD, client, &ev);
			}
			else
			{
				bool bTerminate = false;
				if((events[i].events & EPOLLERR || events[i].events & EPOLLHUP))
					bTerminate = true;
				else if(events[i].events & EPOLLIN && !processClientRead(fd))
					bTerminate = true;
				else if(events[i].events & EPOLLOUT && !processClientWrite(fd))
					bTerminate = true;
				
				if(bTerminate)
				{
					freeClient(fd, ep);
					m_clients.remove(fd);
				}
			}
		}
		
		for(QMap<int, ClientData>::iterator it = m_clients.begin(); it != m_clients.end();)
		{
			if(time(0) - it.value().lastData > SOCKET_TIMEOUT)
			{
				freeClient(it.key(), ep);
				it = m_clients.erase(it);
			}
			else
				it++;
		}
	}
	
	for(QMap<int, ClientData>::iterator it = m_clients.begin(); it != m_clients.end();)
	{
		freeClient(it.key(), ep);
		it = m_clients.erase(it);
	}
	
	close(ep);
}

void HttpService::freeClient(int fd, int ep)
{
	ClientData& data = m_clients[fd];
	delete data.file;
	delete data.buffer;
	epoll_ctl(ep, EPOLL_CTL_DEL, fd, 0);
	close(fd);
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
		
		if(data.incoming.isEmpty())
			data.incoming << QByteArray();
		
		while(bytes)
		{
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
				data.incoming << QByteArray();
				
				bytes -= now;
				start = p + 1;
			}
		}
	}
	
	for(int i=0;i<data.incoming.size();i++)
	{
		if(data.incoming[i] == "\r\n")
		{
			serveClient(fd);
			while(i >= 0)
				data.incoming.removeAt(i--);
			i = 0;
		}
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
			QByteArray buf = data.file->read(8*1024);
			
			return write(fd, buf.constData(), buf.size()) == buf.size();
		}
	}
	else if(data.buffer && !data.buffer->isEmpty())
	{
		QByteArray ar;
		unsigned long bytes = 8*1024;
		
		ar.resize(bytes);
		data.buffer->getData(ar.data(), &bytes);
		
		return write(fd, ar.constData(), bytes) == int(bytes);
	}
	
	return true;
}

bool HttpService::authenitcate(const QList<QByteArray>& data)
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

void HttpService::serveClient(int fd)
{
	bool bAuthFail = false;
	QByteArray fileName, queryString;
	ClientData& data = m_clients[fd];
	
	if(data.incoming[0].startsWith("GET "))
	{
		//bSendBody = true;
		int end = data.incoming[0].lastIndexOf(' ');
		fileName = data.incoming[0].mid(4, end-4);
	}
	/*else if(data.incoming[0].startsWith("HEAD "))
	{
		bSendBody = false;
		int end = data.incoming[0].lastIndexOf(' ');
		fileName = data.incoming[0].mid(5, end-5);
	}*/
	
	if(fileName.isEmpty() || fileName.indexOf("..") >= 0)
	{
		const char* msg = "HTTP/1.0 400 Bad Request\r\n" HTTP_HEADERS "\r\n";
		write(fd, msg, strlen(msg));
		return;
	}
	
	int q = fileName.indexOf('?');
	if(q != -1)
	{
		queryString = fileName.mid(q+1);
		fileName.resize(q);
	}
	
	if(fileName == "/")
		fileName = "/index.html";
	
	fileName.prepend(DATA_LOCATION "/data/remote");
	
	qint64 fileSize = -1;
	if(!fileName.endsWith(".qsp"))
	{
		qDebug() << "Opening" << fileName;
		
		data.file = new QFile(fileName);
		
		if(data.file->open(QIODevice::ReadOnly))
			fileSize = data.file->size();
	}
	else
	{
		if(authenitcate(data.incoming))
		{
			QFile file(fileName);
			if(file.open(QIODevice::ReadOnly))
			{
				data.buffer = new OutputBuffer;
				
				qDebug() << "Executing" << fileName;
				interpretScript(&file, data.buffer, queryString);
				fileSize = data.buffer->size();
			}
		}
		else
			bAuthFail = true;
	}
	
	char buffer[4096];
	if(bAuthFail)
	{
		strcpy(buffer, "HTTP/1.0 401 Unauthorized\r\nWWW-Authenticate: Basic realm=\"FatRat web interface\""
				"\r\nContent-Length: 16\r\n" HTTP_HEADERS "\r\n401 Unauthorized");
	}
	else if(fileSize != -1)
	{
		sprintf(buffer, "HTTP/1.0 200 OK\r\n" HTTP_HEADERS "Content-Length: %lld\r\n"
				"Cache-Control: no-cache\r\nPragma: no-cache\r\n\r\n", fileSize);
	}
	else
		strcpy(buffer, "HTTP/1.0 404 Not Found\r\nContent-Length: 13\r\n" HTTP_HEADERS "\r\n404 Not Found");
	
	qDebug() << buffer;
	write(fd, buffer, strlen(buffer));
	
	processClientWrite(fd);
}

QVariantMap HttpService::processQueryString(QByteArray queryString)
{
	QVariantMap map;
	QStringList s = QUrl::fromPercentEncoding(queryString).split('&');
	foreach(QString e, s)
	{
		int p = e.indexOf('=');
		if(p < 0)
			continue;
		map[e.left(p)] = e.mid(p+1);
	}
	return map;
}

QScriptValue pagePrintFunction(QScriptContext* context, QScriptEngine* engine)
{
	QString result;
	for(int i = 0; i < context->argumentCount(); i++)
	{
		if(i > 0)
			result.append(' ');
		result.append(context->argument(i).toString());
	}

	QScriptValue calleeData = context->callee().data();
	OutputBuffer* buffer = qobject_cast<OutputBuffer*>(calleeData.toQObject());
	QByteArray b = result.toUtf8();
	
	buffer->putData(b.constData(), b.size());

	return engine->undefinedValue();
}

QScriptValue formatSizeFunction(QScriptContext* context, QScriptEngine* engine)
{
	qulonglong size;
	bool persec;
	
	if(context->argumentCount() != 2)
	{
		context->throwError("formatSize(): wrong argument count");
		return engine->undefinedValue();
	}
	
	size = context->argument(0).toNumber();
	persec = context->argument(1).toBoolean();
	
	return engine->toScriptValue(formatSize(size, persec));
}

QScriptValue formatTimeFunction(QScriptContext* context, QScriptEngine* engine)
{
	qulonglong secs;
	
	if(context->argumentCount() != 1)
	{
		context->throwError("formatTime(): wrong argument count");
		return engine->undefinedValue();
	}
	
	secs = context->argument(0).toNumber();
	
	return engine->toScriptValue(formatTime(secs));
}

QScriptValue transferSpeedFunction(QScriptContext* context, QScriptEngine* engine)
{
	if(context->argumentCount() != 0)
	{
		context->throwError("Transfer.speed(): wrong argument count");
		return engine->undefinedValue();
	}
	Transfer* t = (Transfer*) context->thisObject().toQObject();
	int down, up;
	t->speeds(down, up);
	
	QScriptValue v = engine->newObject();
	v.setProperty("down", engine->toScriptValue(down));
	v.setProperty("up", engine->toScriptValue(up));
	
	return v;
}

QScriptValue transferTimeLeftFunction(QScriptContext* context, QScriptEngine* engine)
{
	if(context->argumentCount() != 0)
	{
		context->throwError("Transfer.speed(): wrong argument count");
		return engine->undefinedValue();
	}
	
	Transfer* t = (Transfer*) context->thisObject().toQObject();
	Transfer::Mode mode = t->primaryMode();
	QString time;
	int down,up;
	qint64 total, done;
	
	t->speeds(down,up);
	total = t->total();
	done = t->done();
	
	if(total)
	{
		if(mode == Transfer::Download && down)
			time = formatTime((total-done)/down);
		else if(mode == Transfer::Upload && up)
			time = formatTime((total-done)/up);
	}
	
	return engine->toScriptValue(time);
}

Q_DECLARE_METATYPE(Queue*)
QScriptValue queueToScriptValue(QScriptEngine *engine, Queue* const &in)
{
	return engine->newQObject(in);
}

void queueFromScriptValue(const QScriptValue &object, Queue* &out)
{
	out = qobject_cast<Queue*>(object.toQObject());
}

Q_DECLARE_METATYPE(Transfer*)
QScriptValue transferToScriptValue(QScriptEngine *engine, Transfer* const &in)
{
	QScriptValue retval = engine->newQObject(in);
	retval.setProperty("speed", engine->newFunction(transferSpeedFunction));
	retval.setProperty("timeLeft", engine->newFunction(transferTimeLeftFunction));
	
	return retval;
}

void transferFromScriptValue(const QScriptValue &object, Transfer* &out)
{
	out = qobject_cast<Transfer*>(object.toQObject());
}

void HttpService::interpretScript(QFile* input, OutputBuffer* output, QByteArray queryString)
{
	QScriptEngine engine;
	QByteArray in;
	QScriptValue fun;
	QVariantMap gets = processQueryString(queryString);
	
	g_queuesLock.lockForWrite();
	in = input->readAll();
	
	engine.globalObject().setProperty("GET", engine.toScriptValue(gets));
	
	QScriptValue queues = engine.newArray(g_queues.size());
	for(int i=0;i<g_queues.size();i++)
		queues.setProperty(i, engine.newQObject(g_queues[i]));
	engine.globalObject().setProperty("QUEUES", queues);
	
	fun = engine.newFunction(pagePrintFunction);
	fun.setData(engine.newQObject(output));
	engine.globalObject().setProperty("print", fun);
	
	fun = engine.newFunction(formatSizeFunction);
	engine.globalObject().setProperty("formatSize", fun);
	
	fun = engine.newFunction(formatTimeFunction);
	engine.globalObject().setProperty("formatTime", fun);
	
	qScriptRegisterMetaType(&engine, queueToScriptValue, queueFromScriptValue);
	qScriptRegisterMetaType(&engine, transferToScriptValue, transferFromScriptValue);
	
	int p = 0;
	while(true)
	{
		int e,next = in.indexOf("<?", p);
		int line;
		
		output->putData(in.constData()+p, (next>=0) ? next-p : in.size()-p);
		if(next < 0)
			break;
		
		line = countLines(in, next) + 1;
		next += 2;
		
		e = in.indexOf("?>", next);
		if(e < 0)
			break;
		p = e+2;
		
		engine.evaluate(in.mid(next, e-next), input->fileName(), line);
		if(engine.hasUncaughtException())
		{
			QByteArray errmsg = handleException(&engine);
			output->putData(errmsg.constData(), errmsg.size());
		}
	}
	
	queues = engine.globalObject().property("QUEUES");
	for(int i=g_queues.size();i>=0;i--)
	{
		QScriptValue v = queues.property(i);
		if(v.isNull() || v.isUndefined())
		{
			// destroy the queue
			delete g_queues.takeAt(i);
		}
	}
	g_queuesLock.unlock();
}

QByteArray HttpService::handleException(QScriptEngine* engine)
{
	QByteArray errmsg = "<div style=\"color: red; border: 1px solid red\"><p><b>QtScript runtime exception:</b> <code>";
	errmsg += engine->uncaughtException().toString().toUtf8();
	errmsg += "</code>";
	
	int line = engine->uncaughtExceptionLineNumber();
	if(line != -1)
	{
		errmsg += " on line ";
		errmsg += QByteArray::number(line);
	}
	
	errmsg += "</p><p>Backtrace:</p><pre>";
	errmsg += engine->uncaughtExceptionBacktrace().join("\n");
	errmsg += "</pre></div>";
	
	return errmsg;
}

int HttpService::countLines(const QByteArray& ar, int left)
{
	int lines = 0, p = 0;
	while(true)
	{
		p = ar.indexOf('\n', p+1);
		if(p > left || p < 0)
			break;
		lines++;
	}
	return lines;
}

