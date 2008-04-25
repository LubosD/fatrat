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
