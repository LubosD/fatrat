/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#ifndef JABBERSERVICE_H
#define JABBERSERVICE_H
#include "config.h"
#include <QThread>
#include <QList>
#include <QStringList>
#include <QUuid>
#include <QDateTime>

#ifndef WITH_JABBER
#	error This file is not supposed to be included!
#endif

#include <gloox/client.h>
#include <gloox/messagehandler.h>
#include <gloox/chatstatehandler.h>
#include <gloox/chatstatefilter.h>
#include <gloox/connectionlistener.h>
#include <gloox/rosterlistener.h>

class Transfer;
class QSocketNotifier;

class JabberService : public QObject, public gloox::MessageHandler, public gloox::ConnectionListener, public gloox::ChatStateHandler, public gloox::RosterListener
{
Q_OBJECT
public:
	JabberService();
	~JabberService();
	
	static JabberService* instance() { return m_instance; }
	static std::string qstring2stdstring(QString str);
	void applySettings();
	
	// gloox::MessageHandler
#if defined(GLOOX_1_0)
	virtual void handleMessage(const gloox::Message& message, gloox::MessageSession* session = 0);
#elif defined(GLOOX_0_9)
	virtual void handleMessage(gloox::Stanza* stanza, gloox::MessageSession* session = 0);
#endif
	void handleMessageGeneric(gloox::Stanza* stanza, gloox::MessageSession* session, QString body);
	
	// gloox::ConnectionListener
	virtual void onConnect();
	virtual void onDisconnect(gloox::ConnectionError e);
	virtual void onResourceBindError(gloox::ResourceBindError error);
	virtual void onSessionCreateError(gloox::SessionCreateError error);
	virtual bool onTLSConnect(const gloox::CertInfo &info) { return true; }
	virtual void onStreamEvent(gloox::StreamEvent event) {}
	
	// gloox::ChatStateHandler
	virtual void handleChatState(const gloox::JID &from, gloox::ChatStateType state);
	
	// gloox::RosterListener
	virtual void handleItemAdded (const gloox::JID &jid) {}
	virtual void handleItemSubscribed (const gloox::JID &jid) {}
	virtual void handleItemRemoved (const gloox::JID &jid) {}
	virtual void handleItemUpdated (const gloox::JID &jid) {}
	virtual void handleItemUnsubscribed (const gloox::JID &jid) {}
	virtual void handleRoster (const gloox::Roster &roster) {}
	virtual bool handleSubscriptionRequest(const gloox::JID& jid, const std::string& msg) { return m_bGrantAuth; }
	virtual bool handleUnsubscriptionRequest(const gloox::JID& jid, const std::string& msg) { return m_bGrantAuth; }
#if defined(GLOOX_0_9)
	virtual void handleRosterPresence(const gloox::RosterItem & item, const std::string& resource, gloox::Presence presence, const std::string& msg) {}
	virtual void handleSelfPresence(const gloox::RosterItem & item, const std::string& resource, gloox::Presence presence, const std::string& msg) {}
	virtual void handleNonrosterPresence (gloox::Stanza *stanza) {}
	virtual void handleRosterError (gloox::Stanza *stanza) {}
#elif defined(GLOOX_1_0)
	virtual void handleRosterPresence(const gloox::RosterItem & item, const std::string& resource, gloox::Presence::PresenceType presenceType, const std::string& msg) {}
	virtual void handleSelfPresence(const gloox::RosterItem & item, const std::string& resource, gloox::Presence::PresenceType presence, const std::string& msg) {}
	virtual void handleNonrosterPresence (const gloox::Presence& presence) {}
	virtual void handleRosterError (const gloox::IQ& iq) {}
#endif
protected:
	struct ConnectionInfo
	{
		QString strJID, strThread;
		int nQueue;
		gloox::ChatStateFilter* chatState;
		QDateTime lastActivity;
		
		bool operator==(const ConnectionInfo& other) const
		{
			return strJID == other.strJID && strThread == other.strThread;
		}
	};
	
	ConnectionInfo* getConnection(gloox::MessageSession* session, gloox::Stanza* stanza = 0);
	ConnectionInfo* createConnection(gloox::MessageSession* session);
	QString processCommand(ConnectionInfo* conn, QString cmd);
	void validateQueue(ConnectionInfo* conn);
	static QStringList parseCommand(QString input, QString* extargs = 0);
	static QString transferInfo(Transfer* t);
private slots:
	void socketActivated();
	void run();
private:
	QString m_strJID, m_strPassword, m_strResource;
	bool m_bRestrictSelf, m_bRestrictPassword;
	QString m_strRestrictPassword;
	bool m_bGrantAuth;
	int m_nPriority;
	QUuid m_proxy;
	
	gloox::Client* m_pClient;
	QSocketNotifier* m_pNotifier;
	bool m_bTerminating;
	static JabberService* m_instance;
	
	QList<ConnectionInfo> m_connections;
};

#endif
