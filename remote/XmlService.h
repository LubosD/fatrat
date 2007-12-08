#ifndef XMLSERVICE_H
#define XMLSERVICE_H
#include "GenericService.h"
#include <QXmlDefaultHandler>

class XmlService : public GenericService, public QXmlDefaultHandler
{
public:
	XmlService();
	bool startElement(const QString& nspace, const QString& localName, const QString& qName, const QXmlAttributes& atts);
	bool characters(const QString& data);
protected:
	void processClient(QTcpSocket* client);
	void sendResponse(QString cmd, QString id, QString data);
	
	bool m_bLoggedIn;
	QTcpSocket* m_client;
	QString m_strLastCommand, m_strLastID;
};

#endif
