#ifndef JAVASERVICE_H
#define JAVASERVICE_H
#include "GenericService.h"

class JavaService
{
public:
	void processClient(QTcpSocket* client);
	bool processCommand(QByteArray data, QTcpSocket* socket);
	void sendResponse(QStringList args, QTcpSocket* socket);
private:
	bool m_bLoggedIn;
	QString m_salt;
};

#endif
