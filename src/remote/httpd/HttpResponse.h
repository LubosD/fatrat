#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H
#include <QString>
#include <cstring>
#include "HttpDaemon.h"

class HttpResponse
{
protected:
	HttpResponse(HttpDaemon* daemon, int id);
public:
	// Sends a generic error page
	void sendErrorGeneric(int code, QString desc);

	void sendResponseSimple(int code, QString desc, const char* data, size_t len);
	inline void sendResponseSimple(int code, QString desc, const char* data) { sendResponseSimple(code, desc, data, strlen(data)); }
	void sendResponseCallback(int code, QString desc);

	void sendResponseAsyncStart(int code, QString desc);
	void sendResponseAsyncData(const char* data, size_t len);
	inline void sendResponseAsyncData(const char* data) { sendResponseAsyncData(data, strlen(data)); }

	// Will drop the connection if Content-Length was specified and sendResponseAsyncEnd() was called prematurely
	void sendResponseAsyncEnd();

	// Add a HTTP response header
	void addHeader(QString name, QString value);
private:
	HttpDaemon* m_daemon;
	int m_id;

	friend class HttpDaemon;
};

#endif

