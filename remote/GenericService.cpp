#include "GenericService.h"
#include <QTcpServer>
#include <qatomic.h>

GenericService::GenericService()
	: m_bAbort(false)
{
}

GenericService::~GenericService()
{
	m_bAbort = true;
	wait();
}

void GenericService::run()
{
	QTcpServer server;
	
	if(!server.listen(QHostAddress::Any, m_nPort))
	{
		qDebug() << "Failed to listen on the port" << m_nPort;
		return;
	}
	
	while(!m_bAbort)
	{
		if(server.waitForNewConnection(1000))
		{
			QTcpSocket* client = server.nextPendingConnection();
			
			processClient(client);
			
			client->flush();
			client->close();
			delete client;
		}
	}
}
