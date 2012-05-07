#ifndef FR_MHDSERVICE_H
#define FR_MHDSERVICE_H
#include <QString>
#include "MHDConnection.h"

class MHDService
{
public:
	virtual ~MHDService() {}

	virtual void handleGet(QString url, MHDConnection& conn)
	{
		conn.showErrorPage(MHD_HTTP_METHOD_NOT_ALLOWED, "Method Not Allowed");
	}

	virtual void handlePost(QString url, MHDConnection& conn, const QByteArray& postData)
	{
		conn.showErrorPage(MHD_HTTP_METHOD_NOT_ALLOWED, "Method Not Allowed");
	}

	virtual void handleHead(QString url, MHDConnection& conn)
	{
		conn.showErrorPage(MHD_HTTP_METHOD_NOT_ALLOWED, "Method Not Allowed");
	}
};

#endif

