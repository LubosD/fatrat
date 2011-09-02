/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2010 Lubos Dolezel <lubos a dolezel.info>

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

#include "config.h"
#include "Transfer.h"
#include "Settings.h"
#include "Queue.h"

#ifdef WITH_BITTORRENT
#	include "engines/TorrentDownload.h"
#endif

#ifdef ENABLE_FAKEDOWNLOAD
#	include "engines/FakeDownload.h"
#endif

#ifdef WITH_CURL
#	include "engines/CurlDownload.h"
#	include "engines/CurlUpload.h"
#	include "engines/MetalinkDownload.h"
#endif

#include <QtDebug>
#include <QDir>
#include <QMessageBox>
#include <QVariant>
#include <QProcess>

Q_GLOBAL_STATIC(TransferNotifier, transferNotifier);

QVector<EngineEntry> g_enginesDownload;
QVector<EngineEntry> g_enginesUpload;

extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;

void initTransferClasses()
{
#ifdef ENABLE_FAKEDOWNLOAD
	{ // couldn't look more lazy and lame
		EngineEntry e = { "FakeDownload", "Fake engine", 0, 0, { FakeDownload::createInstance }, { FakeDownload::acceptable }, 0, 0 };
		g_enginesDownload << e;
	}
#endif
#ifdef WITH_BITTORRENT
	{
		EngineEntry e = { "TorrentDownload", "BitTorrent download", TorrentDownload::globalInit, TorrentDownload::globalExit, { TorrentDownload::createInstance }, { TorrentDownload::acceptable }, 0 };
		g_enginesDownload << e;
	}
#endif
#ifdef WITH_CURL
	{
		EngineEntry e = { "GeneralDownload", "CURL HTTP(S)/FTP(S)/SFTP download", CurlDownload::globalInit, CurlDownload::globalExit, { CurlDownload::createInstance }, { CurlDownload::acceptable }, CurlDownload::createMultipleOptionsWidget };
		g_enginesDownload << e;
	}
	{
		EngineEntry e = { "FtpUpload", "CURL FTP(S)/SFTP upload", 0, 0, { CurlUpload::createInstance }, { CurlUpload::acceptable }, 0 };
		g_enginesUpload << e;
	}
	{
		EngineEntry e = { "MetalinkDownload", "Metalink file handler", MetalinkDownload::globalInit, 0, { MetalinkDownload::createInstance }, { MetalinkDownload::acceptable }, 0 };
		g_enginesDownload << e;
	}
#endif
}

void addTransferClass(const EngineEntry& e, Transfer::Mode m)
{
	if(m == Transfer::Download)
		g_enginesDownload << e;
	else if(m == Transfer::Upload)
		g_enginesUpload << e;
}

Transfer::Transfer(bool local)
	: m_state(Paused), m_mode(Download), m_nDownLimit(0), m_nUpLimit(0),
		  m_nDownLimitInt(0), m_nUpLimitInt(0), m_bLocal(local), m_bWorking(false),
		  m_nTimeRunning(0), m_nRetryCount(0)
{
	m_uuid = QUuid::createUuid();
}

Transfer::~Transfer()
{
}

Transfer::State Transfer::state() const
{
	return m_state;
}

void Transfer::setUserSpeedLimits(int down,int up)
{
	m_nDownLimitInt = m_nDownLimit = down;
	m_nUpLimitInt = m_nUpLimit = up;
	setSpeedLimits(down,up);
}

void Transfer::setInternalSpeedLimits(int down,int up)
{
	if((m_nDownLimit < down && m_nDownLimit) || !down)
		down = m_nDownLimit;
	if((m_nUpLimit < up && m_nUpLimit) || !up)
		up = m_nUpLimit;
	
	if(down != m_nDownLimitInt || up != m_nUpLimitInt)
	{
		m_nDownLimitInt = down;
		m_nUpLimitInt = up;
	
		setSpeedLimits(down,up);
	}
}

bool Transfer::isActive() const
{
	return m_state == Active || m_state == ForcedActive;
}

Transfer* Transfer::createInstance(QString className)
{
	qDebug() << "Transfer::createInstance():" << className;
	for(int i=0;i<g_enginesDownload.size();i++)
	{
		if(className == g_enginesDownload[i].shortName)
			return g_enginesDownload[i].lpfnCreate2(&g_enginesDownload[i]);
	}
	for(int i=0;i<g_enginesUpload.size();i++)
	{
		if(className == g_enginesUpload[i].shortName)
			return g_enginesUpload[i].lpfnCreate2(&g_enginesUpload[i]);
	}
	
	return 0;
}

int Transfer::getEngineID(QString className, Mode type)
{
	if (type == Download)
	{
		for(int i=0;i<g_enginesDownload.size();i++)
		{
			if(className == g_enginesDownload[i].shortName)
				return i;
		}
	}
	else if (type == Upload)
	{
		for(int i=0;i<g_enginesUpload.size();i++)
		{
			if(className == g_enginesUpload[i].shortName)
				return i;
		}
	}
	
	return -1;
}

const char* Transfer::getEngineName(int id, Mode type)
{
	const EngineEntry* e = engines(type);
	return e[id].shortName;
}

Transfer* Transfer::createInstance(Mode mode, int classID)
{
	const EngineEntry* entries = engines(mode);
	
	return entries[classID].lpfnCreate2(&entries[classID]);
}

bool Transfer::runProperties(QWidget* parent, Mode mode, int classID, QList<Transfer*> objects)
{
	const EngineEntry* entries = engines(mode);
	bool err = false;

	if(!entries[classID].lpfnMultiOptions)
	    err = true;
	if(!err)
	{
		int result;
		QDialog* dlg = entries[classID].lpfnMultiOptions(parent, objects);

		if(dlg)
		{
		    result = dlg->exec();

		    delete dlg;
		    return result == QDialog::Accepted;
		}
		else
		    err = true;
	}
	if(err)
		QMessageBox::information(parent, "FatRat", tr("This transfer type has no advanced options to set."));
	return true;
}

const EngineEntry* Transfer::engines(Mode type)
{
	return (type == Download) ? g_enginesDownload.constData() : g_enginesUpload.constData();
}

Transfer::BestEngine Transfer::bestEngine(QString uri, Mode type)
{
	int curscore = 0;
	BestEngine best;
	
	if(type != Upload)
	{
		for(int i=0;i<g_enginesDownload.size();i++)
		{
			int n;
			
			if(!g_enginesDownload[i].lpfnAcceptable)
				continue;
			
			n = g_enginesDownload[i].lpfnAcceptable2(uri, type == ModeInvalid, &g_enginesDownload[i]);
			
			if(n > curscore)
			{
				curscore = n;
				
				best.engine = &g_enginesDownload[i];
				best.nClass = i;
				best.type = Download;
			}
		}
	}
	if(type != Download)
	{
		for(int i=0;i<g_enginesUpload.size();i++)
		{
			int n;
			
			if(!g_enginesUpload[i].lpfnAcceptable)
				continue;
			
			n = g_enginesUpload[i].lpfnAcceptable2(uri, type == ModeInvalid, &g_enginesUpload[i]);
			
			if(n > curscore)
			{
				curscore = n;
				
				best.engine = &g_enginesUpload[i];
				best.nClass = i;
				best.type = Upload;
			}
		}
	}
	
	return best;
}

void Transfer::setState(State newState)
{
	bool now,was = isActive();
	State oldState = m_state;
	
	if(newState == oldState)
		return;
	
	enterLogMessage(tr("Changed state: %1 -> %2").arg(state2string(m_state)).arg(state2string(newState)));
	
	if(newState == Completed)
		fireCompleted();
	
	m_state = newState;
	now = isActive();
	
	if(now != was)
	{
		m_bWorking = false;
		changeActive(now);
		
		if(now)
			m_timeStart = QDateTime::currentDateTime();
		else
			m_nTimeRunning += m_timeStart.secsTo(QDateTime::currentDateTime());
	}
	
	if(!m_bLocal)
		emit TransferNotifier::instance()->stateChanged(this, oldState, newState);
	emit stateChanged(m_state, newState);
}

qint64 Transfer::timeRunning() const
{
	if(!isActive())
		return m_nTimeRunning;
	else
		return m_nTimeRunning + m_timeStart.secsTo(QDateTime::currentDateTime());
}

Transfer::State Transfer::string2state(QString s)
{
#define IFCASE(x) if(s == #x) return (x)
	IFCASE(Waiting);
	else IFCASE(Active);
	else IFCASE(ForcedActive);
	else IFCASE(Paused);
	else IFCASE(Failed);
	else IFCASE(Completed);
	else return Paused;
#undef IFCASE
}

QString Transfer::state2string(State s)
{
#define DOCASE(s) case (s): return #s
	switch(s)
	{
	DOCASE(Waiting);
	DOCASE(Active);
	DOCASE(ForcedActive);
	DOCASE(Paused);
	DOCASE(Failed);
	DOCASE(Completed);
	default: return "Paused";
	}
#undef DOCASE
}

void Transfer::load(const QDomNode& map)
{
	int down = 0, up = 0;
	
	setState(string2state(getXMLProperty(map, "state")));
	down = getXMLProperty(map, "downlimit").toInt();
	up = getXMLProperty(map, "uplimit").toInt();
	m_strComment = getXMLProperty(map, "comment");
	m_nTimeRunning = getXMLProperty(map, "timerunning").toLongLong();
	m_uuid = getXMLProperty(map, "uuid");
	
	if(m_uuid.isNull())
		m_uuid = QUuid::createUuid();
	
	QDomElement n = map.firstChildElement("action");
	while(!n.isNull())
	{
		if(n.attribute("state") == "Completed")
			m_strCommandCompleted = n.firstChild().toText().data();
		n = n.nextSiblingElement("action");
	}
	
	setUserSpeedLimits(down, up);
}

void Transfer::save(QDomDocument& doc, QDomNode& node) const
{
	setXMLProperty(doc, node, "state", state2string(m_state));
	setXMLProperty(doc, node, "downlimit", QString::number(m_nDownLimit));
	setXMLProperty(doc, node, "uplimit", QString::number(m_nUpLimit));
	setXMLProperty(doc, node, "comment", m_strComment);
	setXMLProperty(doc, node, "timerunning", QString::number(timeRunning()));
	setXMLProperty(doc, node, "uuid", m_uuid.toString());
	
	QDomElement elem = doc.createElement("action");
	QDomText text = doc.createTextNode(m_strCommandCompleted);
	elem.setAttribute("state", "Completed");
	elem.appendChild(text);
	
	node.appendChild(elem);
}

void Transfer::setMode(Mode newMode)
{
	if(!m_bLocal)
		emit TransferNotifier::instance()->modeChanged(this, m_mode, newMode);
	emit modeChanged(m_mode, newMode);
	m_mode = newMode;
}

void Transfer::updateGraph()
{
	int down, up;
	
	speeds(down,up);
	
	if(m_qSpeedData.size() >= getSettingsValue("graphminutes").toInt()*60)
		m_qSpeedData.dequeue();
	m_qSpeedData.enqueue(QPair<int,int>(down,up));
}

QString Transfer::getXMLProperty(const QDomNode& node, QString name)
{
	QDomNode n = node.firstChildElement(name);
	if(n.isNull())
		return QString();
	else
		return n.firstChild().toText().data();
}

void Transfer::setXMLProperty(QDomDocument& doc, QDomNode& node, QString name, QString value)
{
	QDomElement sub = doc.createElement(name);
	QDomText text = doc.createTextNode(value);
	sub.appendChild(text);
	node.appendChild(sub);
}

QString Transfer::dataPath(bool bDirect) const
{
	QString obj = object();
	
	if(primaryMode() == Download)
	{
		if(bDirect)
			return QDir(obj).filePath(name());
		else
			return obj;
	}
	else
	{
		if(bDirect)
			return obj;
		else
		{
			QDir dir(obj);
			dir.cdUp();
			return dir.absolutePath();
		}
	}
}

void Transfer::retry()
{
	m_nRetryCount++;
	setState(Waiting);
}

void Transfer::fireCompleted()
{
	if(m_strCommandCompleted.isEmpty())
		return;
	
	QString exec = m_strCommandCompleted;
	for(int i=0;i<exec.size() - 1;)
	{
		if(exec[i++] != '%')
			continue;
		QChar t = exec[i];
		QString text;
		
		if(t == QChar('N')) // transfer name
			text = name();
		else if(t == QChar('T')) // transfer type
			text = myClass();
		else if(t == QChar('D')) // destination directory
			text = dataPath(false);
		else if(t == QChar('P')) // data path
			text = dataPath(true);
		else
			continue;
		
		exec.replace(i-1, 2, text);
		i += text.size() - 1;
	}
	
	qDebug() << "Executing" << exec;
	
	QProcess::startDetached(exec);
}

QString Transfer::autoActionCommand(State state) const
{
	if(state == Completed)
		return m_strCommandCompleted;
	return QString();
}

void Transfer::setAutoActionCommand(State state, QString command)
{
	if(state == Completed)
		m_strCommandCompleted = command;
}

QString Transfer::stateString() const
{
	return state2string(state());
}

void Transfer::setStateString(QString s)
{
	setState(string2state(s));
}

void Transfer::setSpeedLimits(int down,int up)
{
}

QString Transfer::uuid() const
{
	return m_uuid.toString();
}

void Transfer::replaceItself(Transfer* newObject)
{
	QReadLocker l(&g_queuesLock);
	foreach (Queue* q, g_queues)
	{
		if (q->contains(this))
		{
			q->replace(this, newObject);
			break;
		}
	}
}

void Transfer::replaceItself(Transfer::TransferList newObjects)
{
	QReadLocker l(&g_queuesLock);
	foreach (Queue* q, g_queues)
	{
		if (q->contains(this))
		{
			q->replace(this, newObjects);
			break;
		}
	}
}

Queue* Transfer::myQueue() const
{
	QReadLocker l(&g_queuesLock);
	foreach (Queue* q, g_queues)
	{
		if (q->contains(const_cast<Transfer*>(this)))
			return q;
	}
	return 0;
}

//////////////////

TransferNotifier::TransferNotifier()
{
	qRegisterMetaType<Transfer::State>("Transfer::State");
	qRegisterMetaType<Transfer::Mode>("Transfer::Mode");
}

TransferNotifier* TransferNotifier::instance()
{
	return transferNotifier();
}
