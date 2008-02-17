#include "JabberService.h"
#include "fatrat.h"
#include "Queue.h"
#include "RuntimeException.h"

#include <gloox/messagesession.h>
#include <gloox/disco.h>
#include <QSettings>
#include <QtDebug>

extern QSettings* g_settings;
extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;

JabberService* JabberService::m_instance = 0;

JabberService::JabberService()
	: m_bTerminating(false)
{
	m_instance = this;
	applySettings();
}

JabberService::~JabberService()
{
	if(isRunning())
	{
		m_bTerminating = true;
		m_pClient->disconnect();
		wait();
	}
	
	m_instance = 0;
}

void JabberService::applySettings()
{
	qDebug() << "Loading Jabber settings";
	
	m_strJID = g_settings->value("jabber/jid").toString();
	m_strPassword = g_settings->value("jabber/password").toString();
	m_bRestrictSelf = g_settings->value("jabber/restrict_self", getSettingsDefault("jabber/restrict_self")).toBool();
	m_bRestrictPassword = g_settings->value("jabber/restrict_password_bool").toBool();
	m_strRestrictPassword = g_settings->value("jabber/restrict_password").toString();
	m_nPriority = g_settings->value("jabber/priority", getSettingsDefault("jabber/priority")).toInt();
	
	if(g_settings->value("jabber/enabled", getSettingsDefault("jabber/enabled")).toBool())
	{
		qDebug() << "Jabber is enabled";
		if(!isRunning())
			start();
		else
			; // TODO: compare and possibly restart
	}
	else if(isRunning())
	{
		m_bTerminating = true;
		m_pClient->disconnect();
	}
}

void JabberService::run()
{
	qDebug() << "Thread is running";
	while(!m_bTerminating)
	{
		gloox::JID jid( (m_strJID + "/FatRat").toStdString());
		
		m_pClient = new gloox::Client(jid, m_strPassword.toStdString());
		m_pClient->registerMessageHandler(this);
		//m_pClient->registerConnectionListener(this);
		m_pClient->disco()->addFeature(gloox::XMLNS_CHAT_STATES);
		m_pClient->disco()->setIdentity("client", "bot");
		m_pClient->disco()->setVersion("FatRat", VERSION);
		m_pClient->setPresence(gloox::PresenceAvailable, m_nPriority);
		
		m_pClient->connect(); // this function returns only in case of failure
		
		if(!m_bTerminating)
			sleep(5);
	}
}

void JabberService::handleMessage(gloox::Stanza* stanza, gloox::MessageSession* session)
{
	ConnectionInfo* conn = getConnection(session, stanza);
	QString message = QString::fromUtf8( stanza->body().c_str() );
	bool bAuthed = conn != 0;
	
	gloox::JID from = stanza->from();
	if(from.resource() == "FatRat")
		return;
	
	if(!conn)
	{
		session = new gloox::MessageSession(m_pClient, from);
		session->registerMessageHandler(this);
	}
	
	qDebug() << "Session ID is" << session->threadID().c_str();
	
	if(!bAuthed)
	{
		if(!m_bRestrictPassword)
		{
			if(m_bRestrictSelf)
				bAuthed = from.bare() == m_strJID.toStdString();
			else
				bAuthed = true;
		}
		
		if(bAuthed)
			createConnection(session);
	}
	
	QString msg;
	if(!bAuthed)
	{
		if(!message.startsWith("pass "))
		{
			msg = tr("This is a FatRat remote control bot.\nYou are not authorized. "
					"You may login using a password, if enabled - send:\n\npass yourpassword");
		}
		else
		{
			bool bAccepted = m_bRestrictPassword;
			bAccepted &= m_strRestrictPassword == message.mid(5);
			
			if(bAccepted)
			{
				msg = tr("Password accepted, send \"help\" for list of commands.");
				createConnection(session);
			}
			else
				msg = tr("Password rejected.");
		}
		session->send( qstring2stdstring(msg) );
	}
	else
	{
		if(message == "logout" || message == "exit" || message == "quit")
		{
			msg = tr("Bye.");
			conn->chatState->setChatState(gloox::ChatStateGone);
			session->send( msg.toStdString() );
			m_pClient->disposeMessageSession(session);
	
			m_connections.removeAll(*conn);
		}
		else
		{
			msg = processCommand(conn, message);
			session->send( qstring2stdstring(msg) );
		}
	}
}

QStringList JabberService::parseCommand(QString input)
{
	QStringList list;
	
	/*for(int i=0;i<input.size();i++)
	{
		
	}*/
	
	list = input.split(' ');
	
	return list;
}

QString JabberService::processCommand(ConnectionInfo* conn, QString cmd)
{
	QStringList args = parseCommand(cmd);
	QString response;
	
	try
	{
		if(args[0] == "help")
		{
			response = tr("List of commands:\nqlist - Show list of queues\n"
					"qset - Set current queue ID\n"
					"list - Show transfers of the current queue\n"
					"pauseall/resumeall - Pause/resume all transfers\n"
					"pause/resume/delete - Pause/resume/delete specified transfers\n"
					"logout/quit/exit - Log out\n"
					"\nPass arguments like this: \"resume 1 3 5\", use indexes from the lists");
		}
		else if(args[0] == "qlist")
		{
			QReadLocker locker(&g_queuesLock);
			response = tr("List of queues:");
			
			if(!g_queues.isEmpty())
			{
				for(int i=0;i<g_queues.size();i++)
				{
					const Queue::Stats& stats = g_queues[i]->m_stats;
					response += tr("\n#%1 - \"%2\"; %3/%4 active; %5 down, %6 up").arg(i).arg(g_queues[i]->name())
							.arg(stats.active_d+stats.active_u).arg(g_queues[i]->size())
							.arg(formatSize(stats.down, true)).arg(formatSize(stats.up, true));
				}
			}
			else
				response += tr("no queues");
		}
		else if(args[0] == "qset")
		{
			int q = args[1].toInt();
			if(q >= 0 && q < g_queues.size())
			{
				conn->nQueue = q;
				response = tr("OK.");
			}
			else
				response = tr("Invalid queue ID.");
		}
		else if(args[0] == "list")
		{
			validateQueue(conn);
			
			QReadLocker locker(&g_queuesLock);
			response = tr("List of transfers:");
			
			Queue* q = g_queues[conn->nQueue];
			q->lock();
			
			if(q->size())
			{
				for(int i=0;i<q->size();i++)
					response += tr("\n#%1 %2").arg(i).arg(transferInfo(q->at(i)));
			}
			else
				response += tr("no transfers");
			
			q->unlock();
		}
		else if(args[0] == "pauseall" || args[0] == "resumeall")
		{
			validateQueue(conn);
			QReadLocker locker(&g_queuesLock);
			Transfer::State state;
			
			if(args[0] == "resumeall")
				state = Transfer::Active;
			else if(args[0] == "pauseall")
				state = Transfer::Paused;
			
			Queue* q = g_queues[conn->nQueue];
			for(int i=0;i<q->size();i++)
				q->at(i)->setState(state);
		}
		else if(args[0] == "pause" || args[0] == "resume")
		{
			validateQueue(conn);
			QReadLocker locker(&g_queuesLock);
			Queue* q = g_queues[conn->nQueue];
			
			Transfer::State state;
			if(args[0] == "pause")
				state = Transfer::Paused;
			else if(args[0] == "resume")
				state = Transfer::Active;
			
			response = tr("Set transfer states:");
			
			q->lock();
			
			for(int i=1;i<args.size();i++)
			{
				int id = args[i].toInt();
				
				if(id >= 0 && id < q->size())
				{
					q->at(id)->setState(state);
					response += tr("\n#%1 %2").arg(id).arg(transferInfo(q->at(id)));
				}
				else
					response += tr("\n#%1 Invalid transfer ID").arg(id);
			}
			
			q->unlock();
		}
		else if(args[0] == "remove")
		{
			validateQueue(conn);
			QReadLocker locker(&g_queuesLock);
			Queue* q = g_queues[conn->nQueue];
			QList<int> items;
			
			for(int i=1;i<args.size();i++)
			{
				items << args[i].toInt();
			}
			
			qSort(items);
			
			response = tr("Removing transfers");
			
			for(int i=items.size()-1;i>=0;i--)
			{
				int index = items[i];
				if(index >= 0 && index < q->size())
					q->remove(index);
				else
					response += tr("\n#%1 Invalid transfer ID").arg(index);
			}
		}
		else
		{
			response = tr("Unknown command");
		}
	}
	catch(const RuntimeException& e)
	{
		response = e.what();
	}
	
	qDebug() << response;
	
	return response;
}

QString JabberService::transferInfo(Transfer* t)
{
	int down, up;
	float percent;
	qint64 size, done;
	
	t->speeds(down, up);
	size = t->total();
	done = t->done();
	
	if(size)
		percent = 100.f/size*done;
	else
		percent = 0;
	
	return tr("[%2] - \"%3\"; %5 down, %6 up; %7% out of %8").arg(t->state2string(t->state())).arg(t->name())
			.arg(formatSize(down, true)).arg(formatSize(up, true)).arg(percent).arg(formatSize(size, false));
}

void JabberService::validateQueue(ConnectionInfo* conn)
{
	if(conn->nQueue >= g_queues.size() && conn->nQueue >= 0)
		throw RuntimeException( tr("Invalid queue ID.") );
}

std::string JabberService::qstring2stdstring(QString str)
{
	QByteArray data = str.toUtf8();
	return data.data();
}

JabberService::ConnectionInfo* JabberService::createConnection(gloox::MessageSession* session)
{
	ConnectionInfo info;
	gloox::JID from = session->target();
	info.strJID = QString::fromUtf8( from.full().c_str() );
	info.strThread = QString::fromUtf8( session->threadID().c_str() );
	info.nQueue = (g_queues.isEmpty()) ? -1 : 0;
	info.chatState = new gloox::ChatStateFilter(session);
	
	info.chatState->registerChatStateHandler(this);
	
	m_connections << info;
	return &m_connections.last();
}

JabberService::ConnectionInfo* JabberService::getConnection(gloox::MessageSession* session, gloox::Stanza* stanza)
{
	ConnectionInfo* rval = 0;
	
	if(session != 0)
	{
		QString remote = (stanza != 0) ? stanza->from().full().c_str() : session->target().full().c_str();
		for(int i=0;i<m_connections.size();i++)
		{
			qDebug() << "Processing" << remote;
			if(m_connections[i].strJID == remote)
			{
				if(m_connections[i].strThread.toStdString() == session->threadID() || m_connections[i].strThread.isEmpty())
				{
					rval = &m_connections[i];
					rval->strThread = session->threadID().c_str();
					break;
				}
			}
		}
	}
	
	return rval;
}

void JabberService::handleChatState(const gloox::JID &from, gloox::ChatStateType state)
{
}

void JabberService::onConnect()
{
}

void JabberService::onDisconnect(gloox::ConnectionError e)
{
}

void JabberService::onResourceBindError(gloox::ResourceBindError error)
{
}

void JabberService::onSessionCreateError(gloox::SessionCreateError error)
{
}
