#ifndef GENERICSERVICE_H
#define GENERICSERVICE_H
#include <QThread>
#include <QTcpSocket>

class GenericService : public QThread
{
public:
	GenericService();
	~GenericService();
	virtual void run();
protected:
	virtual void processClient(QTcpSocket* client) = 0;
	
	quint16 m_nPort;
	bool m_bAbort;
};

#endif
