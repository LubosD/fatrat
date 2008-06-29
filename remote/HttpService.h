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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
USA.
*/

#ifndef HTTPSERVICE_H
#define HTTPSERVICE_H
#include "config.h"
#include <QTcpServer>
#include <QThread>
#include <QMap>
#include <QByteArray>
#include <QFile>
#include <ctime>

#ifndef WITH_JAVAREMOTE
#	error This file is not supposed to be included!
#endif

class HttpService : public QThread
{
Q_OBJECT
public:
	HttpService();
	~HttpService();
	
	void setup();
	static void throwErrno();
	void run();
	bool processClientRead(int fd);
	bool processClientWrite(int fd);
	
	void freeClient(int fd, int ep);
	
	struct ClientData
	{
		ClientData() : file(0), lastData(time(0)) {}
		QList<QByteArray> incoming;
		QFile* file;
		time_t lastData;
	};
	void serveClient(int fd);
private:
	int m_server;
	bool m_bAbort;
	
	QMap<int, ClientData> m_clients;
};


#endif
