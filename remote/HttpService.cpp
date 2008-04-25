#include "config.h"

#include "HttpService.h"
#include <QSettings>
#include <QtDebug>
#include <QStringList>
#include <QFile>
#include <QTcpSocket>
//#include "JavaService.h"
#include "RuntimeException.h"
#include "fatrat.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

extern QSettings* g_settings;
static const int SOCKET_TIMEOUT = 10;

#define HTTP_HEADERS "Server: FatRat/" VERSION "\r\n"

HttpService::HttpService()
	: m_bAbort(false)
{
	try
	{
		setup();
		start();
	}
	catch(const RuntimeException& e)
	{
		qDebug() << e.what();
	}
}

HttpService::~HttpService()
{
	m_bAbort = true;
	wait();
	close(m_server);
}

void HttpService::setup()
{
	quint16 port;
	bool bIPv6 = true;
	
	port = g_settings->value("remote/port", getSettingsDefault("remote/port")).toUInt();
	
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
	
	if(!data.file)
		return true;
	
	if(!data.file->atEnd() && data.file->isOpen())
	{
		QByteArray buf = data.file->read(8*1024);
		
		return write(fd, buf.constData(), buf.size()) == buf.size();
	}
	return true;
}

void HttpService::serveClient(int fd)
{
	bool bSendBody;
	QString fileName;
	ClientData& data = m_clients[fd];
	
	if(data.incoming[0].startsWith("GET "))
	{
		bSendBody = true;
		int end = data.incoming[0].lastIndexOf(' ');
		fileName = data.incoming[0].mid(4, end-4);
	}
	else if(data.incoming[0].startsWith("HEAD "))
	{
		bSendBody = false;
		int end = data.incoming[0].lastIndexOf(' ');
		fileName = data.incoming[0].mid(5, end-5);
	}
	
	if(fileName.isEmpty() || fileName.indexOf("..") >= 0)
	{
		const char* msg = "HTTP/1.0 400 Bad Request\r\n" HTTP_HEADERS "\r\n";
		write(fd, msg, strlen(msg));
		return;
	}
	
	if(fileName == "/")
		fileName = "/index.html";
	
	fileName.prepend(DATA_LOCATION "/data/remote");
	
	qDebug() << "Opening" << fileName;
	
	data.file = new QFile(fileName);
	data.file->open(QIODevice::ReadOnly);
	
	char buffer[4096];
	
	if(data.file->isOpen())
	{
		qint64 fileSize = data.file->size();
		sprintf(buffer, "HTTP/1.0 200 OK\r\n" HTTP_HEADERS "Content-Length: %lld\r\n\r\n", fileSize);
	}
	else
	{
		strcpy(buffer, "HTTP/1.0 404 Not Found\r\n" HTTP_HEADERS "\r\n");
	}
	
	qDebug() << buffer;
	write(fd, buffer, strlen(buffer));
	
	processClientWrite(fd);
}
