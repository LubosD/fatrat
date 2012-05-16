#ifndef HTTPDAEMON_H
#define HTTPDAEMON_H
#include <QObject>
#include <QMap>
#include <pthread.h>
#include <sys/socket.h>
#include <boost/shared_ptr.hpp>
#include "poller/Poller.h"

class HttpHandler;

class HttpDaemon : public QObject
{
Q_OBJECT
public:
	HttpDaemon();
	virtual ~HttpDaemon();

	void setUseV6(bool usev6) { if (m_server <= 0) m_bUseV6 = usev6; }
	void setBindAddress(QString addr) { if (m_server <= 0) m_strBindAddress = addr; }
	void setPort(unsigned short port) { if (m_server <= 0) m_port = port; }

	virtual void start();
	virtual void stop();
private:
	void startV4();
	void startV6();
	bool pollCycle();
	void acceptClient();
	void closeClient(int s);
	void readClient(int s);
	void writeClient(int s);
	bool tryProcessRequest(int s);
	static QMap<QString,QString> parseUE(QByteArray ba);
	virtual int readBytes(int s, char* buffer, size_t max);
	virtual int writeBytes(int s, const char* buffer, size_t b);
	static void throwErrnoException();
	static void* pollThread(void* t);
private:
	int m_server;
	Poller* m_poller;
	bool m_bUseV6;
	QString m_strBindAddress;
	unsigned short m_port;
	pthread_t m_thread;

	struct Client
	{
		Client() { reset(); }

		boost::shared_ptr<sockaddr> addr;
		enum State { ReceivingHeaders, ReceivingBody, ReceivingUEBody, Responding } state;
		HttpHandler* handler;

		// request buffer
		QByteArray requestBuffer;
		long long bodyLength, bodyReceived;

		void reset()
		{
			bodyLength = bodyReceived = 0;
			handler = 0;
			state = ReceivingHeaders;
		}
	};
	struct HttpRequest
	{
		QString method, uri, queryString;
		QMap<QString,QString> headers;
		QMap<QString,QString> getVars, postVars;
	};

	QMap<int,Client> m_clients;
};

#endif



