#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H
#include <QString>
#include <cstring>
#include <QMap>
#include "HttpDaemon.h"

class HttpResponse
{
protected:
	HttpResponse(HttpDaemon* daemon, int id);
public:
	// Sends a generic error page
    void sendErrorGeneric(int code, const char* desc);

	// SIMPLE response
    void sendResponseSimple(int code, const char* desc, const char* data, size_t len);
    inline void sendResponseSimple(int code, const char* desc, const char* data) { sendResponseSimple(code, desc, data, strlen(data)); }

	// CALLBACK response
    void sendResponseCallback(int code, const char* desc);

	// ASYNC response
    void sendResponseAsyncStart(int code, const char* desc);
	void sendResponseAsyncData(const char* data, size_t len);
	inline void sendResponseAsyncData(const char* data) { sendResponseAsyncData(data, strlen(data)); }
		// Will drop the connection if Content-Length was specified and sendResponseAsyncEnd() was called prematurely
	void sendResponseAsyncEnd();

	// Add a HTTP response header
	void addHeader(QString name, QString value);

	inline bool wasHandled() const { return m_bHandled; }
private:
	void markHandled();
private:
	HttpDaemon* m_daemon;
	int m_id;
    QMap<QString,QString> m_headers;
	bool m_bHandled;

	friend class HttpDaemon;
};

#endif

