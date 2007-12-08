#include "XmlService.h"
#include <QXmlInputSource>
#include <QSettings>
#include "fatrat.h"

extern QSettings* g_settings;

XmlService::XmlService()
	: m_bLoggedIn(false), m_client(0)
{
	m_nPort = g_settings->value("remote/port", getSettingsDefault("remote/port")).toUInt() + 1;
	start();
}

void XmlService::processClient(QTcpSocket* client)
{
	QXmlInputSource src(client);
	QXmlSimpleReader reader;
	
	m_client = client;
	
	reader.setContentHandler(this);
	
	client->write("<fatrat>");
	client->flush();
	reader.parse(src);
}

void XmlService::sendResponse(QString cmd, QString id, QString data)
{
	QString output = QString("<%1 id=\"%2\">%3</%1>").arg(cmd).arg(id).arg(data);
	m_client->write(output.toUtf8());
	m_client->flush();
}

bool XmlService::startElement(const QString&, const QString&, const QString& qName, const QXmlAttributes& attrs)
{
	if(m_strLastID.isEmpty())
	{
		if(m_strLastCommand == "fatrat")
			sendResponse("fatrat", m_strLastID, QString());
		else
		{
			m_strLastCommand = qName;
			m_strLastID = attrs.value("id");
		}
	}
	
	return true;
}

bool XmlService::characters(const QString& data)
{
	return true;
}
