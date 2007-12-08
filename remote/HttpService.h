#ifndef HTTPSERVICE_H
#define HTTPSERVICE_H
#include <QTcpSocket>
#include "GenericService.h"

class HttpService : public GenericService
{
public:
	HttpService();
protected:
	virtual void processClient(QTcpSocket* client);
};

#endif
