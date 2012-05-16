#ifndef HTTPHANDLER_H
#define HTTPHANDLER_H
#include "HttpDaemon.h"

class HttpHandler
{
public:
	virtual void handle(const HttpDaemon::Request& req, HttpResponse* resp, void** p) = 0;
	virtual size_t read(void** p, char* buf, size_t max) { return 0; }
	virtual size_t write(void** p, const char* buf, size_t bytes) { return 0; }
};

#endif

