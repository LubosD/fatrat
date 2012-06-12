#ifndef HTTPDAEMON_H
#define HTTPDAEMON_H
#include <QObject>
#include <QMap>
#include <QList>
#include "HttpRequest.h"
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

    void registerHandler(QString regexp, HttpHandler* handler);

	virtual void start();
	virtual void stop();
protected:
	void startV4();
	void startV6();
	bool pollCycle();

    // Accepts a client connection and allocates required structures
	void acceptClient();

    // Drops the connection and frees all associated resources
	void closeClient(int s);

    // When PollIn is indicated
	void readClient(int s);

    // When PollOut is indicated
	void writeClient(int s);

    // Detect whether we have received a complete request (except for possible request body)
	bool tryProcessRequest(int s);

    // Parses URL-encoded form data
	static QMap<QString,QString> parseUE(QByteArray ba);

    // Reads data from a client socket
	virtual int readBytes(int s, char* buffer, int max);

	int readBytesOrRemnant(int s, char* buffer, int max);

    // Writes data into a client socket
	virtual int writeBytes(int s, const char* buffer, int b);

	HttpHandler* findHandler(QString uri);

    // Throws a RuntimeException with an error msg based on errno
	static void throwErrnoException();
	static void* pollThread(void* t);

	static void parseHTTPRequest(HttpRequest& out, const QByteArray& req);
	static void setFdUnblocking(int fd, bool unblocking);
protected:
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
		QByteArray requestBuffer, remnantBuffer;
        long long requestBodyLength, requestBodyReceived;
        long long responseBodyLength, responseBodySent;

		void* userPointer;
        HttpRequest request;

		void reset()
		{
            requestBodyLength = requestBodyReceived = 0;
            responseBodyLength = responseBodySent = 0;
			handler = 0;
			state = ReceivingHeaders;
			userPointer = 0;
            request = HttpRequest();
		}
	};

    struct Handler
    {
        QString regexp;
        HttpHandler* handler;
    };

	QMap<int,Client> m_clients;
    QList<Handler> m_handlers;

	friend class HttpResponse;
};

#endif



