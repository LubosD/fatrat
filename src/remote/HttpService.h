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

#ifndef HTTPSERVICE_H
#define HTTPSERVICE_H
#include "config.h"
#include "engines/OutputBuffer.h"
#include <QTcpServer>
#include <QThread>
#include <QMap>
#include <QByteArray>
#include <QVariantMap>
#include <QFile>
#include <ctime>

#ifndef WITH_WEBINTERFACE
#	error This file is not supposed to be included!
#endif

struct HttpRequest
{
	HttpRequest() : contentLength(0) { }
	
	QList<QByteArray> lines;
	QByteArray postData;
	long contentLength; // remaining length to read
};
struct ClientData
{
	ClientData() : file(0), buffer(0), lastData(time(0)) {}
	
	QList<HttpRequest> requests;
	// incoming lines
	//QByteArray incoming;
	QList<QByteArray> incoming;
	
	// file to send
	QFile* file;
	
	// script output to send
	OutputBuffer* buffer;
	
	// last reception/send
	time_t lastData;
};

class Queue;
class Transfer;
class Poller;

class HttpService : public QThread
{
Q_OBJECT
public:
	HttpService();
	~HttpService();
	static HttpService* instance() { return m_instance; }
	void applySettings();
	
	void setup();
	static void throwErrno();
	void run();
	
	static QMap<QString,QString> processQueryString(QByteArray queryString);
	
	static int countLines(const QByteArray& ar, int left);
	static bool authenticate(const QList<QByteArray>& data);
	
	static long contentLength(const QList<QByteArray>& data);
	
	static QByteArray graph(QString queryString);
	static QByteArray qgraph(QString queryString);
	static QString urlDecode(QByteArray arr);
	static void findQueue(QString queueUUID, Queue** q);
	static int findTransfer(QString transferUUID, Queue** q, Transfer** t, bool lockForWrite = false);
private:
	bool processClientRead(int fd);
	bool processClientWrite(int fd);
	void freeClient(int fd, Poller* poller);
	void serveClient(int fd);
	void initScriptEngine();
	void timet2lastModified(time_t t, char* buffer, size_t size);
	static QByteArray copyrights();
	static QByteArray copyLog(QString uuidTransfer = QString());
	static QByteArray listDirectory(QString uuidTransfer, QString path);
private:
	static HttpService* m_instance;
	int m_server;
	bool m_bAbort;
	
	QMap<int, ClientData> m_clients;
};


#endif
