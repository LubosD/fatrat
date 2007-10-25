#include "Transfer.h"
#include "FakeDownload.h"
#include "GeneralDownload.h"
#include "TorrentDownload.h"
#include "FtpUpload.h"
#include <iostream>
#include <QtDebug>
#include <QMessageBox>

Q_GLOBAL_STATIC(TransferNotifier, transferNotifier)

using namespace std;

extern QSettings* g_settings;

static const EngineEntry m_enginesDownload[] = {
	{ "FakeDownload", "Fake engine", 0, FakeDownload::createInstance, FakeDownload::acceptable, 0, 0 },
	{ "GeneralDownload", "HTTP/FTP download", 0, GeneralDownload::createInstance, GeneralDownload::acceptable, GeneralDownload::createSettingsWidget, GeneralDownload::createMultipleOptionsWidget },
	{ "TorrentDownload", "BitTorrent download", TorrentDownload::globalInit, TorrentDownload::createInstance, TorrentDownload::acceptable, TorrentDownload::createSettingsWidget, 0 },
	{0,0,0,0,0,0,0}
};

static const EngineEntry m_enginesUpload[] = {
	{ "FtpUpload", "FTP upload", 0, FtpUpload::createInstance, FtpUpload::acceptable, 0, 0 },
	{0,0,0,0,0,0,0}
};

Transfer::Transfer(bool local)
	: m_state(Waiting), m_mode(Download), m_nDownLimit(0), m_nUpLimit(0), m_bLocal(local)
{
	m_timer = new QTimer(this);
	
	connect(m_timer, SIGNAL(timeout()), this, SLOT(updateGraph()));
	m_timer->start(1000);
}

Transfer::~Transfer()
{
}

void Transfer::setUserSpeedLimits(int down,int up)
{
	m_nDownLimit = down;
	m_nUpLimit = up;
	setSpeedLimits(down,up);
}

void Transfer::setInternalSpeedLimits(int down,int up)
{
	if(m_nDownLimit)
		down = m_nDownLimit;
	if(m_nUpLimit)
		up = m_nUpLimit;
	setSpeedLimits(down,up);
}

bool Transfer::isActive() const
{
	return m_state == Active || m_state == ForcedActive;
}

Transfer* Transfer::createInstance(QString className)
{
	for(size_t i=0;i<sizeof(m_enginesDownload)/sizeof(m_enginesDownload[0]);i++)
	{
		if(className == m_enginesDownload[i].shortName)
			return m_enginesDownload[i].lpfnCreate();
	}
	for(size_t i=0;i<sizeof(m_enginesUpload)/sizeof(m_enginesUpload[0]);i++)
	{
		if(className == m_enginesUpload[i].shortName)
			return m_enginesUpload[i].lpfnCreate();
	}
	
	return 0;
}

Transfer* Transfer::createInstance(Mode mode, int classID)
{
	const EngineEntry* entries = engines(mode);
	
	return entries[classID].lpfnCreate();
}

bool Transfer::runProperties(QWidget* parent, Mode mode, int classID, QList<Transfer*> objects)
{
	const EngineEntry* entries = engines(mode);
	if(!entries[classID].lpfnMultiOptions)
	{
		QMessageBox::information(parent, "FatRat", tr("This transfer type has no advanced options to set."));
		return true;
	}
	else
	{
		int result;
		QDialog* dlg = entries[classID].lpfnMultiOptions(parent, objects);
		
		result = dlg->exec();
		
		delete dlg;
		return result == QDialog::Accepted;
	}
}

const EngineEntry* Transfer::engines(Mode type)
{
	return (type == Download) ? m_enginesDownload : m_enginesUpload;
}

bool Transfer::statePossible(State newState)
{
	if(m_state == newState)
		return false;
	if(newState != ForcedActive && m_state == Completed)
	{
		//if(done() >= total())
		//	return false;
		//else
		//	cout << "WTF, done: " << done() << " but total: " << total() << endl;
	}
	return true;
}

void Transfer::setState(State newState)
{
	bool now,was = isActive();
	
	if(!statePossible(newState))
		return;
	
	if(!m_bLocal)
		emit TransferNotifier::instance()->stateChanged(this, m_state, newState);
	emit stateChanged(m_state, newState);
	
	enterLogMessage(tr("Changed state: %1 -> %2").arg(state2string(m_state)).arg(state2string(newState)));
	
	m_state = newState;
	now = isActive();
	
	if(now != was)
		changeActive(now);
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
	
	setUserSpeedLimits(down, up);
}

void Transfer::save(QDomDocument& doc, QDomNode& node)
{
	setXMLProperty(doc, node, "state", state2string(m_state));
	setXMLProperty(doc, node, "downlimit", QString::number(m_nDownLimit));
	setXMLProperty(doc, node, "uplimit", QString::number(m_nUpLimit));
	setXMLProperty(doc, node, "comment", m_strComment);
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
	
	if(m_qSpeedData.size() >= g_settings->value("graphminutes",int(5)).toInt()*60)
		m_qSpeedData.dequeue();
	m_qSpeedData.enqueue(QPair<int,int>(down,up));
}

void Transfer::enterLogMessage(QString msg)
{
	QString text = QString("%1 %2 - %3")
			.arg( QDate::currentDate().toString(Qt::ISODate) )
			.arg( QTime::currentTime().toString(Qt::ISODate) )
			.arg(msg);
	emit logMessage(text);
	if(!m_strLog.isEmpty())
		m_strLog += '\n';
	
	m_strLog += text;
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
