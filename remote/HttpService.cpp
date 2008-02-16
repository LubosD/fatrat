#include "config.h"

#include "HttpService.h"
#include <QSettings>
#include <QtDebug>
#include <QStringList>
#include <QFile>
#include "fatrat.h"

extern QSettings* g_settings;

#define HTTP_HEADERS "Connection: close\r\nServer: FatRat/" VERSION "\r\n"

HttpService::HttpService()
{
	m_nPort = g_settings->value("remote/port", getSettingsDefault("remote/port")).toUInt();
	start();
}

void HttpService::processClient(QTcpSocket* client)
{
	QStringList lines;
	bool bSendBody;
	QString fileName;
	QFile file;
	
	while(true)
	{
		char line[256];
		
		qint64 read = client->readLine(line, sizeof line);
		
		if(read < 0)
		{
			if(!client->waitForReadyRead(10*1000))
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
		client->write("HTTP/1.0 400 Bad Request\r\n" HTTP_HEADERS "\r\n");
		return;
	}
	
	if(fileName == "/")
		fileName = "/index.html";
	
	qDebug() << "File name" << fileName;
	
	if(fileName.indexOf(":") < 0 && fileName.indexOf(".."))
	{
		fileName.prepend(DATA_LOCATION "/data/remote");
		file.setFileName(fileName);
		file.open(QIODevice::ReadOnly);
	}
	
	if(!file.isOpen())
	{
		client->write("HTTP/1.0 404 Not Found\r\n" HTTP_HEADERS "\r\n");
	}
	else
	{
		qint64 fileSize = file.size();
		
		client->write( QString("HTTP/1.0 200 OK\r\n" HTTP_HEADERS
				"Content-Length: %1\r\n\r\n").arg(fileSize).toUtf8());
		
		if(bSendBody)
		{
			const int BUFSIZE = 32*1024;
			char buffer[BUFSIZE];
			qint64 sent = 0;
			
			while(sent < fileSize)
			{
				qint64 thisTime = qMin<qint64>(fileSize-sent, BUFSIZE);
				file.read(buffer, thisTime);
				client->write(buffer, thisTime);
				sent += thisTime;
			}
		}
	}
}
