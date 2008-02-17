#ifndef JABBERSERVICE_H
#define JABBERSERVICE_H
#include "config.h"
#include <QThread>
#include <QList>
#include <QStringList>

#ifndef WITH_JABBER
#	error This file is not supposed to be included!
#endif

#include <gloox/client.h>
#include <gloox/messagehandler.h>
#include <gloox/chatstatehandler.h>
#include <gloox/chatstatefilter.h>
#include <gloox/connectionlistener.h>

class Transfer;

class JabberService : public QThread, public gloox::MessageHandler, public gloox::ConnectionListener, public gloox::ChatStateHandler
{
Q_OBJECT
public:
	JabberService();
	~JabberService();
	
	static JabberService* instance() { return m_instance; }
	static std::string qstring2stdstring(QString str);
	void applySettings();
	
	virtual void run();
	virtual void handleMessage(gloox::Stanza* stanza, gloox::MessageSession* session = 0);
	
	virtual void onConnect();
	virtual void onDisconnect(gloox::ConnectionError e);
	virtual void onResourceBindError(gloox::ResourceBindError error);
	virtual void onSessionCreateError(gloox::SessionCreateError error);
	virtual bool onTLSConnect(const gloox::CertInfo &info) { return true; }
	virtual void onStreamEvent(gloox::StreamEvent event) {}
	
	virtual void handleChatState(const gloox::JID &from, gloox::ChatStateType state);
protected:
	struct ConnectionInfo
	{
		QString strJID, strThread;
		int nQueue;
		gloox::ChatStateFilter* chatState;
		
		bool operator==(const ConnectionInfo& other) const
		{
			return strJID == other.strJID && strThread == other.strThread;
		}
	};
	
	ConnectionInfo* getConnection(gloox::MessageSession* session, gloox::Stanza* stanza = 0);
	ConnectionInfo* createConnection(gloox::MessageSession* session);
	QString processCommand(ConnectionInfo* conn, QString cmd);
	void validateQueue(ConnectionInfo* conn);
	static QStringList parseCommand(QString input);
	static QString transferInfo(Transfer* t);
private:
	QString m_strJID, m_strPassword;
	bool m_bRestrictSelf, m_bRestrictPassword;
	QString m_strRestrictPassword;
	int m_nPriority;
	
	gloox::Client* m_pClient;
	bool m_bTerminating;
	static JabberService* m_instance;
	
	QList<ConnectionInfo> m_connections;
};

#endif
