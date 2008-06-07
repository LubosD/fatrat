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

#ifndef WITH_REMOTE
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

class QScriptEngine;
class QScriptValue;

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
	
	QScriptValue convertQueryString(const QMap<QString,QString>& queryString);
	QMap<QString,QString> processQueryString(QByteArray queryString);
	
	static int countLines(const QByteArray& ar, int left);
	static bool authenitcate(const QList<QByteArray>& data);
	
	static long contentLength(const QList<QByteArray>& data);
	
	static QByteArray progressBar(QByteArray queryString);
	static QByteArray graph(QString queryString);
	static QString urlDecode(QByteArray arr);
private:
	void interpretScript(QFile* input, OutputBuffer* output, QByteArray queryString, QByteArray postData);
	QByteArray handleException();
	bool processClientRead(int fd);
	bool processClientWrite(int fd);
	void freeClient(int fd, int ep);
	void serveClient(int fd);
	void initScriptEngine();
private:
	static HttpService* m_instance;
	int m_server;
	bool m_bAbort;
	QScriptEngine* m_engine;
	
	QMap<int, ClientData> m_clients;
};


#endif
