#ifndef HTTPHANDLER_H
#define HTTPHANDLER_H
#include "HttpDaemon.h"
#include "HttpResponse.h"
#include "HttpRequest.h"
#include <boost/shared_ptr.hpp>

class HttpHandler
{
public:
	// Called to handle the request
	// Will throw HTTP Error 500 if returns without an HttpResponse::send* call
    virtual void handle(const HttpRequest& req, boost::shared_ptr<HttpResponse> resp, void** p) = 0;

	// Used with sendResponseCallback()
	// Return 0 to stop the callbacks
	// Will drop the connection if Content-Length was specified and 0 was returned prematurely
    virtual size_t read(void** userData, const HttpRequest& req, char* buf, size_t max) { return 0; }

	// Used with incoming POST data, called N times before handle()
	// Will drop the connection if return value != bytes
    virtual size_t write(void** userData, const HttpRequest& req, const char* buf, size_t bytes) { return 0; }
};

#endif

