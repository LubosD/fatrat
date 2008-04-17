#include "config.h"

#include "HttpService.h"
#include <QSettings>
#include <QtDebug>
#include <QStringList>
#include <QFile>
#include <QTcpSocket>
#include "JavaService.h"
#include "fatrat.h"

extern QSettings* g_settings;
static const int MAX_THREADS = 20;

#define HTTP_HEADERS "Connection: close\r\nServer: FatRat/" VERSION "\r\n"

HttpService::HttpService()
	: m_nThreads(0)
{
	quint16 port;
	port = g_settings->value("remote/port", getSettingsDefault("remote/port")).toUInt();
	
	if(!this->listen(QHostAddress::Any, port))
	{
		qDebug() << "Failed to listen on the port" << port;
		return;
	}
}

void HttpService::incomingConnection(int sock)
{
	if(m_nThreads >= MAX_THREADS)
	{
		QTcpSocket socket;
		socket.setSocketDescriptor(sock);
		socket.close();
	}
	else
	{
		m_nThreads++;
		HttpThread* thread = new HttpThread(sock);
		connect(thread, SIGNAL(finished()), this, SLOT(threadFinished()));
		thread->start();
	}
}

void HttpService::threadFinished()
{
	m_nThreads--;
}

void HttpThread::run()
{
	QStringList lines;
	bool bSendBody;
	QString fileName;
	QFile file;
	
	QTcpSocket sock;
	sock.setSocketDescriptor(m_socket);
	
	while(true)
	{
		char line[256];
		
		qint64 read = sock.readLine(line, sizeof line);
		
		if(read < 0)
		{
			if(!sock.waitForReadyRead(10*1000))
			{
				qDebug() << "HTTP timeout";
				return;
			}
		}
		else
		{
			QByteArray trimmed = QByteArray(line, read).trimmed();
			if(trimmed.isEmpty())
				break;
			lines << trimmed;
		}
	}
	
	if(lines[0].startsWith("GET "))
	{
		bSendBody = true;
		int end = lines[0].lastIndexOf(' ');
		fileName = lines[0].mid(4, end-4);
	}
	else if(lines[0].startsWith("HEAD "))
	{
		bSendBody = false;
		int end = lines[0].lastIndexOf(' ');
		fileName = lines[0].mid(5, end-5);
	}
	else
	{
		sock.write("HTTP/1.0 400 Bad Request\r\n" HTTP_HEADERS "\r\n");
		return;
	}
	
	if(fileName == "/")
		fileName = "/index.html";
	
	qDebug() << "File name" << fileName;
	
	if(fileName == "/service")
	{
		JavaService serv;
		serv.processClient(&sock);
	}
	else
	{
		if(fileName.indexOf("..") < 0)
		{
			fileName.prepend(DATA_LOCATION "/data/remote");
			file.setFileName(fileName);
			file.open(QIODevice::ReadOnly);
		}
		
		if(!file.isOpen())
		{
			sock.write("HTTP/1.0 404 Not Found\r\n" HTTP_HEADERS "\r\n");
		}
		else
		{
			qint64 fileSize = file.size();
			
			sock.write( QString("HTTP/1.0 200 OK\r\n" HTTP_HEADERS
					"Content-Length: %1\r\n\r\n").arg(fileSize).toUtf8());
			
			if(bSendBody)
			{
				const int BUFSIZE = 32*1024;
				qint64 sent = 0;
				
				while(sent < fileSize)
				{
					qint64 thisTime = qMin<qint64>(fileSize-sent, BUFSIZE);
					QByteArray buffer = file.read(thisTime);
					sock.write(buffer);
					sent += thisTime;
				}
				sock.flush();
			}
		}
	}
	
	sock.flush();
	sock.close();
}

HttpThread::HttpThread(int sock)
	: m_socket(sock)
{
	connect(this, SIGNAL(finished()), this, SLOT(deleteLater()));
}
