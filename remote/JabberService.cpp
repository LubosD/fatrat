#include "JabberService.h"
#include "fatrat.h"
#include "Queue.h"
#include "RuntimeException.h"
#include "Logger.h"
#include "dbus/DbusImpl.h"

#include <gloox/messagesession.h>
#include <gloox/rostermanager.h>
#include <gloox/disco.h>
#include <gloox/connectionhttpproxy.h>
#include <gloox/connectionsocks5proxy.h>
#include <gloox/connectiontcpclient.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <QSettings>
#include <QtDebug>

extern QSettings* g_settings;
extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;

const int SESSION_MINUTES = 10;

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
		if(m_pClient != 0)
			m_pClient->disconnect();
		wait();
		delete m_pClient;
	}
	
	m_instance = 0;
}

void JabberService::applySettings()
{
	QString jid, password, resource;
	QUuid proxy;
	int priority;
	bool bChanged = false;
	
	jid = g_settings->value("jabber/jid").toString();
	password = g_settings->value("jabber/password").toString();
	priority = g_settings->value("jabber/priority", getSettingsDefault("jabber/priority")).toInt();
	proxy = g_settings->value("jabber/proxy").toString();
	resource = g_settings->value("jabber/resource", getSettingsDefault("jabber/resource")).toString();
	
	m_bGrantAuth = g_settings->value("jabber/grant_auth", getSettingsDefault("jabber/grant_auth")).toBool();
	m_bRestrictSelf = g_settings->value("jabber/restrict_self", getSettingsDefault("jabber/restrict_self")).toBool();
	m_bRestrictPassword = g_settings->value("jabber/restrict_password_bool").toBool();
	m_strRestrictPassword = g_settings->value("jabber/restrict_password").toString();
	
	if(jid != m_strJID)
	{
		bChanged = true;
		m_strJID = jid;
	}
	if(password != m_strPassword)
	{
		bChanged = true;
		m_strPassword = password;
	}
	if(priority != m_nPriority)
	{
		bChanged = true;
		m_nPriority = priority;
	}
	if(proxy != m_proxy)
	{
		bChanged = true;
		m_proxy = proxy;
	}
	if(resource != m_strResource)
	{
		bChanged = true;
		m_strResource = resource;
	}
	
	if(g_settings->value("jabber/enabled", getSettingsDefault("jabber/enabled")).toBool())
	{
		if(!isRunning())
			start();
		else if(bChanged)
		{
			m_bTerminating = true;
			if(m_pClient)
				m_pClient->disconnect();
			wait();
			delete m_pClient;
			start();
		}
	}
	else if(isRunning())
	{
		m_bTerminating = true;
		if(m_pClient)
			m_pClient->disconnect();
		wait();
		delete m_pClient;
		m_pClient = 0;
	}
}

void JabberService::run()
{
	while(!m_bTerminating)
	{
		sleep(2);
		
		gloox::JID jid( (m_strJID + '/' + m_strResource).toStdString());
		
		Logger::global()->enterLogMessage("Jabber", tr("Connecting..."));
		
		m_pClient = new gloox::Client(jid, m_strPassword.toStdString());
		m_pClient->registerMessageHandler(this);
		m_pClient->registerConnectionListener(this);
		m_pClient->rosterManager()->registerRosterListener(this);
		m_pClient->disco()->addFeature(gloox::XMLNS_CHAT_STATES);
		m_pClient->disco()->setIdentity("client", "bot");
		m_pClient->disco()->setVersion("FatRat", VERSION);
		m_pClient->setPresence(gloox::PresenceAvailable, m_nPriority);
		
		gloox::ConnectionBase* proxy = 0;
		Proxy pdata = Proxy::getProxy(m_proxy);
		
		if(pdata.nType == Proxy::ProxyHttp)
		{
			gloox::ConnectionHTTPProxy* p;
			proxy = p = new gloox::ConnectionHTTPProxy(m_pClient,
				new gloox::ConnectionTCPClient(m_pClient->logInstance(), pdata.strIP.toStdString(), pdata.nPort),
                                m_pClient->logInstance(), m_pClient->server(), m_pClient->port());
			
			if(!pdata.strUser.isEmpty())
				p->setProxyAuth(pdata.strUser.toStdString(), pdata.strPassword.toStdString());
		}
		else if(pdata.nType == Proxy::ProxySocks5)
		{
			gloox::ConnectionSOCKS5Proxy* p;
			proxy = p = new gloox::ConnectionSOCKS5Proxy(m_pClient,
				new gloox::ConnectionTCPClient(m_pClient->logInstance(), pdata.strIP.toStdString(), pdata.nPort),
                                m_pClient->logInstance(), m_pClient->server(), m_pClient->port());
			
			if(!pdata.strUser.isEmpty())
				p->setProxyAuth(pdata.strUser.toStdString(), pdata.strPassword.toStdString());
		}
		
		if(proxy != 0)
			m_pClient->setConnectionImpl(proxy);
		
		m_pClient->connect();
		
		if(!m_bTerminating)
		{
			gloox::Client* c = m_pClient;
			m_pClient = 0;
			delete c;
			
			sleep(3);
		}
	}
	m_bTerminating = false;
}

void JabberService::handleMessage(gloox::Stanza* stanza, gloox::MessageSession* session)
{
	ConnectionInfo* conn = getConnection(session, stanza);
	QString message = QString::fromUtf8( stanza->body().c_str() );
	bool bAuthed = conn != 0;
	
	gloox::JID from = stanza->from();
	if(from.resource() == m_strResource.toStdString())
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
			
			if(m_bRestrictSelf)
				bAccepted &= from.bare() == m_strJID.toStdString();
			bAccepted &= m_strRestrictPassword == message.mid(5);
			
			if(bAccepted)
			{
				msg = tr("Password accepted, send \"help\" for the list of commands.");
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
			
			Logger::global()->enterLogMessage("Jabber", tr("%1 logged out").arg(from.full().c_str()));
	
			m_connections.removeAll(*conn);
		}
		else
		{
			msg = processCommand(conn, message);
			session->send( qstring2stdstring(msg) );
		}
	}
}

QStringList JabberService::parseCommand(QString input, QString* extargs)
{
	QStringList list;
	QString tmp;
	
	int i;
	bool bInStr = false, bInQ = false, bExtArgs = false;
	
	// for oversimplified mobile Jabber clients that don't provide any way of writing \n
	// without sending the message
	input.replace("\\n", "\n");
	
	for(i=0;i<input.size();i++)
	{
		if(input[i] == '\n')
		{
			bExtArgs = true;
			break;
		}
		
		if(input[i] == '"')
		{
			bInQ = bInStr = true;
			continue;
		}
		else if(input[i] != ' ')
			bInStr = true;
		
		if(bInStr)
		{
			if((bInQ && input[i] == '"') || (!bInQ && input[i] == ' '))
			{
				if(!tmp.isEmpty() || bInQ)
				{
					list << tmp;
					tmp.clear();
				}
				bInStr = bInQ = false;
			}
			else
			{
				tmp += input[i];
			}
		}
	}
	
	if(!tmp.isEmpty())
		list << tmp;
	if(bExtArgs && extargs != 0)
	{
		*extargs = input.mid(i+1);
	}
	
	qDebug() << list;
	
	return list;
}

QString JabberService::processCommand(ConnectionInfo* conn, QString cmd)
{
	QString extargs;
	QStringList args = parseCommand(cmd, &extargs);
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
					"\nPass arguments like this: \"resume 1 3 5\", use indexes from the lists\n\n"
					"add/new - Add new transfers\n"
					"This command needs special arguments. See more at http://fatrat.dolezel.info/doc/jabber.xhtml");
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
		else if(args[0] == "remove" || args[0] == "delete")
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
		else if(args[0] == "add" || args[0] == "new")
		{
			validateQueue(conn);
			if(extargs.isEmpty())
				response = tr("Nothing to add");
			else
			{
				if(args.size() < 2 || args[1].startsWith('/'))
					args.insert(1, "auto");
				
				if(args.size() < 3)
					args << g_settings->value("defaultdir", getSettingsDefault("defaultdir")).toString();
				
				QMetaObject::invokeMethod(DbusImpl::instance(),
						"addTransfersNonInteractive", Qt::QueuedConnection,
						/*Q_RETURN_ARG(QString, response),*/
						Q_ARG(QString, extargs), Q_ARG(QString, args[2]),
						Q_ARG(QString, args[1]), Q_ARG(int, conn->nQueue));
				
				if(response.isEmpty())
					response = tr("Command accepted, check the queue");
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
	info.lastActivity = QDateTime::currentDateTime();
	
	Logger::global()->enterLogMessage("Jabber", tr("New chat session: %1").arg(info.strJID));
	
	info.chatState->registerChatStateHandler(this);
	
	m_connections << info;
	return &m_connections.last();
}

JabberService::ConnectionInfo* JabberService::getConnection(gloox::MessageSession* session, gloox::Stanza* stanza)
{
	ConnectionInfo* rval = 0;
	
	if(!session)
		return 0;
	
	QString remote = (stanza != 0) ? stanza->from().full().c_str() : session->target().full().c_str();
	for(int i=0;i<m_connections.size();i++)
	{
		qDebug() << "Processing" << remote;
		if(m_connections[i].strJID != remote)
			continue;

		if(m_connections[i].strThread.toStdString() == session->threadID() || m_connections[i].strThread.isEmpty())
		{
			const QDateTime currentTime = QDateTime::currentDateTime();
			
			if(m_connections[i].lastActivity.secsTo(currentTime) > SESSION_MINUTES*60)
			{
				m_connections.removeAt(i--);
			}
			else
			{
				rval = &m_connections[i];
				rval->lastActivity = currentTime;
				rval->strThread = session->threadID().c_str();
				break;
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
	Logger::global()->enterLogMessage("Jabber", tr("Connected"));
	
	gloox::ConnectionTCPBase* base = dynamic_cast<gloox::ConnectionTCPBase*>(m_pClient->connectionImpl());
	
	if(base != 0)
	{
		int sock = base->socket();
		int yes = 1;
		setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof yes);
	}
}

void JabberService::onDisconnect(gloox::ConnectionError e)
{
	QString err = tr("Disconnected:") + ' ';
	
	switch(e)
	{
		case gloox::ConnStreamError:
			err += tr("Stream error"); break;
		case gloox::ConnStreamVersionError:
			err += tr("Stream version error"); break;
		case gloox::ConnStreamClosed:
			err += tr("Stream closed"); break;
		case gloox::ConnProxyAuthRequired:
			err += tr("Proxy authentication required"); break;
		case gloox::ConnProxyAuthFailed:
			err += tr("Proxy authentication failed"); break;
		case gloox::ConnProxyNoSupportedAuth:
			err += tr("The proxy requires an unsupported auth mechanism"); break;
		case gloox::ConnIoError:
			err += tr("I/O error"); break;
		case gloox::ConnParseError:
			err += tr("XML parse error"); break;
		case gloox::ConnConnectionRefused:
			err += tr("Failed to connect"); break;
		case gloox::ConnDnsError:
			err += tr("Failed to resolve the domain name"); break;
		case gloox::ConnOutOfMemory:
			err += tr("Out of memory"); break;
		case gloox::ConnNoSupportedAuth:
			err += tr("The server doesn't provide any supported authentication mechanism"); break;
		case gloox::ConnAuthenticationFailed:
			err += tr("Authentication failed"); break;
		case gloox::ConnUserDisconnected:
			err += tr("The user was disconnect"); break;
		default:
			err += tr("Other reason"); break;
	}
	
	Logger::global()->enterLogMessage("Jabber", err);
}

void JabberService::onResourceBindError(gloox::ResourceBindError error)
{
}

void JabberService::onSessionCreateError(gloox::SessionCreateError error)
{
}
