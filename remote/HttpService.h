#ifndef HTTPSERVICE_H
#define HTTPSERVICE_H
#include "config.h"
#include <QTcpServer>
#include <QThread>

#ifndef WITH_JAVAREMOTE
#	error This file is not supposed to be included!
#endif

class HttpService : public QTcpServer
{
Q_OBJECT
public:
	HttpService();
public slots:
	void threadFinished();
protected:
	void incomingConnection(int sock);
	
	//QTcpServer m_server;
	int m_nThreads;
	
	friend class HttpThread;
};

class HttpThread : public QThread
{
public:
	HttpThread(int sock);
	void run();
private:
	int m_socket;
};

#endif
