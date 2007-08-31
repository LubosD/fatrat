#include "TorrentDownload.h"
#include "torrent/ratecontroller.h"
#include "torrent/connectionmanager.h"
#include <iostream>
#include <QtDebug>
#include <QCoreApplication>
#include "fatrat.h"
#include <QSettings>
#include <QMessageBox>
#include <QObject>
#include <QTemporaryFile>
#include <QHeaderView>
#include "GeneralDownload.h"
#include <stdexcept>

using namespace std;

extern QSettings* g_settings;

TorrentDownload::TorrentDownload() : m_pClient(0), m_nDown(0), m_nUp(0)
{
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(checkRatio()));
	m_timer.start(30*1000);
}

TorrentDownload::~TorrentDownload()
{
	if(m_pClient)
	{
		m_pClient->stop();
		
		delete m_pClient;
	}
}

int TorrentDownload::acceptable(QString uri)
{
	const bool istorrent = uri.endsWith(".torrent", Qt::CaseInsensitive);
	if(uri[0] == '/' || uri.startsWith("http://") || uri.startsWith("ftp://"))
		return (istorrent) ? 3 : 2;
	return 0;
}

void TorrentDownload::init(QString uri,QString target)
{
	if(m_pClient)
		throw std::runtime_error("Internal error: object already initialized");
	
	m_strTarget = target;
	if(uri[0] == '/')
	{
		m_pClient = new TorrentClient;
		m_pClient->moveToThread(QApplication::instance()->thread());
		
		if(!m_pClient->setTorrent(uri))
		{
			QString err = m_pClient->errorString();
			delete m_pClient; m_pClient = 0;
			throw std::runtime_error(err.toStdString());
		}
	
		init();
	}
	else
	{
		QString error,name;
		GeneralDownload* download = new GeneralDownload(true);
		QTemporaryFile* temp = new QTemporaryFile(download);
		QDir dir;
		
		if(!temp->open())
		{
			delete download;
			throw std::runtime_error(tr("Cannot create a temporary file").toStdString());
		}
		
		dir = temp->fileName();
		name = dir.dirName();
		qDebug() << "Downloading torrent to"<<temp->fileName();
		dir.cdUp();
		
		connect(download, SIGNAL(stateChanged(Transfer::State,Transfer::State)), this, SLOT(stateChanged(Transfer::State,Transfer::State)));
		download->init(uri,dir.path());
		download->setTargetName(name);
		download->setState(Active);
		
		m_strMessage = tr("Downloading .torrent file");
	}
}

void TorrentDownload::setObject(QString target)
{
	m_pClient->setDestinationFolder(target); // FIXME: does it work properly?
}

void TorrentDownload::init()
{
	m_pClient->setDestinationFolder(m_strTarget);
	m_pClient->setWantState(m_mapWanted);
	
	m_metaInfo = m_pClient->metaInfo();
	
	connect(m_pClient, SIGNAL(downloadRateUpdated(int)), this, SLOT(downloadRateUpdated(int)));
	connect(m_pClient, SIGNAL(uploadRateUpdated(int)), this, SLOT(uploadRateUpdated(int)));
	connect(m_pClient, SIGNAL(stateChanged(TorrentClient::State)), this, SLOT(clientStateChanged(TorrentClient::State)), Qt::DirectConnection);
	connect(m_pClient, SIGNAL(peerInfoUpdated()), this, SLOT(updateMessage()));
	
	checkRatio();
}

void TorrentDownload::changeActive(bool nowActive)
{
	if(!m_pClient)
	{
		setState(Failed);
		return;
	}
	if(nowActive)
	{
		QMetaObject::invokeMethod(m_pClient, "start", Qt::QueuedConnection);
		updateMessage();
	}
	else
	{
		QMetaObject::invokeMethod(m_pClient, "stop", Qt::QueuedConnection);
		updateMessage();
		m_nDown = m_nUp = 0;
		
		if(state() == Paused)
		{
			double maxRatio = g_settings->value("bittorrent/ratio", 1.0).toDouble();
			if(m_pClient && m_pClient->ratio() >= maxRatio && m_pClient->state() == TorrentClient::Seeding)
			{
				setState(Completed);
			}
		}
	}
}

QString TorrentDownload::name() const
{
	if(m_pClient)
	{
		if(m_metaInfo.fileForm() == MetaInfo::MultiFileForm)
			return m_metaInfo.name();
		else
			return m_metaInfo.singleFile().name;
	}
	else
		return tr("Downloading .torrent file");
}

void TorrentDownload::speeds(int& down, int& up) const
{
	down = m_nDown;
	up = m_nUp;
}

qulonglong TorrentDownload::total() const
{
	return (m_pClient) ? m_metaInfo.totalSize() : 0;
}

qulonglong TorrentDownload::done() const
{
	if(m_pClient)
		return qMin<qulonglong>(m_pClient->done(),total());
	else
		return 0;
}

void TorrentDownload::load(const QDomNode& map)
{
	m_pClient = new TorrentClient;
	m_pClient->moveToThread(QApplication::instance()->thread());
	
	m_strTarget = getXMLProperty(map, "target");
	if(!m_pClient->setTorrent(QByteArray::fromBase64( getXMLProperty(map, "torrentdata").toLatin1() )))
	{
		cerr << "FAILED TO LOAD STORED TORRENT!\n";
		delete m_pClient; m_pClient = 0;
	}
	else
		cout << "Loaded stored torrent\n";
	
	m_pClient->setDumpedState(QByteArray::fromBase64( getXMLProperty(map, "resumedata").toLatin1() ));
	m_pClient->setDownloadedBytes( getXMLProperty(map, "downloadedbytes").toLongLong() );
	m_pClient->setUploadedBytes( getXMLProperty(map, "uploadedbytes").toLongLong() );
	
	QStringList data = getXMLProperty(map, "wantedfiles").split('\\',QString::SkipEmptyParts);
	foreach(QString item,data)
	{
		QStringList contents = item.split(':');
		if(contents.size() != 2)
			continue;
		m_mapWanted[contents[0]] = contents[1].toInt() != 0;
	}
	
	init();
	Transfer::load(map);
	
	if(!m_pClient)
		setState(Failed);
}

void TorrentDownload::save(QDomDocument& doc, QDomNode& map)
{
	Transfer::save(doc, map);
	
	if(m_pClient)
	{
		setXMLProperty(doc, map, "target", object());
		setXMLProperty(doc, map, "torrentdata", QString( m_metaInfo.dataContent().toBase64() ));
		setXMLProperty(doc, map, "resumedata", QString( m_pClient->dumpedState().toBase64() ));
		setXMLProperty(doc, map, "downloadedbytes", QString::number( m_pClient->downloadedBytes() ));
		setXMLProperty(doc, map, "uploadedbytes", QString::number( m_pClient->uploadedBytes() ));
		
		QString data;
		for(QMap<QString,bool>::const_iterator it=m_mapWanted.constBegin();it != m_mapWanted.constEnd(); it++)
		{
			data += QString("%1:%2\\").arg( it.key() ).arg( int(it.value()) );
		}
		setXMLProperty(doc, map, "wantedfiles", data);
	}
}

QString TorrentDownload::object() const
{
	return m_strTarget;
}

void TorrentDownload::setSpeedLimits(int down,int up)
{
	RateController* r = RateController::instance(m_pClient);
	if(r != 0)
	{
		r->setDownloadLimit(down ? down : 1024*1024);
		r->setUploadLimit(up ? up : 1024*1024);
	}
}

void TorrentDownload::updateMessage()
{
	int total = m_pClient->connectedPeerCount();
	int seeds = m_pClient->seedCount();
	
	m_strMessage = QString("L: %1, S: %2; %3").arg(total-seeds).arg(seeds).arg(TorrentClient::state2string(m_pClient->state()));
	
	if(m_pClient->state() == TorrentClient::Preparing)
	{
		if(m_pClient->progress() >= 0)
			m_strMessage += QString(" %1%").arg(m_pClient->progress());
	}
	//qDebug() << m_strMessage;
}

void TorrentDownload::clientStateChanged(TorrentClient::State s)
{
	cout << "TorrentDownload::clientStateChanged()\n";
	updateMessage();
	
	if(s == TorrentClient::Seeding)
	{
		setMode(Upload);
	}
}

void TorrentDownload::checkRatio()
{
	const double ratio = m_pClient->ratio();
	const State mystate = state();
	
	double maxRatio = g_settings->value("bittorrent/ratio", 1.0).toDouble();
	qDebug() << "Checking ratio - maxratio:" << maxRatio << "ratio:" << ratio;
	if(ratio >= maxRatio && mystate == Active)
	{
		if(m_pClient && m_pClient->state() == TorrentClient::Seeding)
			setState(Completed);
	}
	else if(ratio < maxRatio && mystate == Completed)
		setState(Waiting);
}

WidgetHostChild* TorrentDownload::createSettingsWidget(QWidget* w, QIcon& icon)
{
	icon = QIcon(":/fatrat/bittorrent.png");
	return new TorrentDownloadSettings(w);
}

void TorrentDownload::globalInit()
{
	ConnectionManager::instance()->setMaxConnections( g_settings->value("bittorrent/connections").toInt() );
	TorrentClient::setPortRange(g_settings->value("bittorrent/listenstart", 6881).toInt(),
				    g_settings->value("bittorrent/listenend", 6889).toInt());
	TorrentClient::setNumUploadSlots(g_settings->value("bittorrent/uploads",4).toInt());
}

void TorrentDownload::stateChanged(Transfer::State prev, Transfer::State now)
{
	if(prev != Active)
		return;
	
	GeneralDownload* sender = (GeneralDownload*) this->sender();
	
	if(now != Completed)
	{
		qDebug() << "Current state is" << state2string(now);
		setState(Failed);
	}
	else
	{
		qDebug() << "Done downloading .torrent"<<sender->filePath();
		
		try
		{
			init(sender->filePath(),m_strTarget);
			changeActive(isActive());
		}
		catch(const std::exception& e)
		{
			m_strMessage = e.what();
			setState(Failed);
		}
	}
	sender->deleteLater();
}

WidgetHostChild* TorrentDownload::createOptionsWidget(QWidget* w)
{
	return new TorrentOptsWidget(w,this);
}

////////////////////////////////////////////////////////////

TorrentOptsWidget::TorrentOptsWidget(QWidget* me,TorrentDownload* myobj)
	: m_download(myobj)
{
	setupUi(me);
	m_toplevel.item = 0;
	
	QStringList cols;
	
	cols << tr("File name") << tr("File size");
	treeFiles->setHeaderLabels(cols);
	treeFiles->header()->resizeSection(0, 250);
}

void TorrentOptsWidget::updateStatus()
{
	if(m_download->m_pClient)
	{
		sender()->deleteLater();
		fillTree();
	}
	else if(m_download->state() == Transfer::Failed)
	{
		sender()->deleteLater();
		labelStatus->setText( tr("Failed to download torrent") );
	}
}

void TorrentOptsWidget::fillTree()
{
	MetaInfo info = m_download->m_pClient->metaInfo();
	
	labelStatus->setVisible(false);
	
	if(info.fileForm() == MetaInfo::SingleFileForm)
	{
		MetaInfoSingleFile singleFile = info.singleFile();
		treeFiles->addTopLevelItem( new QTreeWidgetItem(treeFiles, QStringList(singleFile.name)) );
	}
	else
	{
		foreach (const MetaInfoMultiFile &entry, info.multiFiles())
		{
			QFlags<Qt::ItemFlag> flags = Qt::ItemIsUserCheckable | Qt::ItemIsSelectable | Qt::ItemIsEnabled;
			Directory& d = getDirectory(entry.path);
			File f;
			QStringList cols;
			
			f.download = true;
			f.name = entry.path.split('/').last();
			qDebug() << "Adding file" << f.name;
			
			cols << f.name;
			cols << formatSize(entry.length);
			f.item = new QTreeWidgetItem(d.item, cols);
			
			if(!d.item)
				treeFiles->addTopLevelItem(f.item);
			
			f.item->setFlags(flags);
			f.item->setCheckState(0,Qt::Checked);
		}
	}
	treeFiles->expandAll();
}

void TorrentOptsWidget::load()
{
	if(m_download->m_pClient)
		fillTree();
	else
	{
		QTimer* timer = new QTimer(this);
		connect(timer, SIGNAL(timeout()), this, SLOT(updateStatus()));
		timer->start(1000);
	}
}

TorrentOptsWidget::Directory& TorrentOptsWidget::getDirectory(QString path)
{
	QStringList structure = path.split('/');
	structure.removeLast();
	
	qDebug() << "Path:" << path;
	
	Directory* now = &m_toplevel;
	for(int i=0;i<structure.size();i++)
	{
		for(int j=0;j<now->dirs.size();j++)
		{
			if(now->dirs[j].name == structure[i])
			{
				now = &now->dirs[j];
				break;
			}
		}
		
		if(now->name != structure[i])
		{
			Directory d;
			d.name = structure[i];
			
			if(now->item != 0)
				d.item = new QTreeWidgetItem(now->item,QStringList(d.name));
			else
			{
				d.item = new QTreeWidgetItem(treeFiles,QStringList(d.name));
				treeFiles->addTopLevelItem(d.item);
			}
			
			qDebug() << "Adding directory" << d.name;
			
			now->dirs << d;
			now = &now->dirs.last();
		}
	}
	return *now;
}

void TorrentOptsWidget::accepted()
{
	QMap<QString,bool> map;
	generateMap(map, m_toplevel, QString());
	m_download->m_mapWanted = map;
	
	if(m_download->m_pClient)
		m_download->m_pClient->setWantState(map);
}

void TorrentOptsWidget::generateMap(QMap<QString,bool>& map, Directory& dir, QString prefix)
{
	foreach(File f,dir.files)
	{
		map[prefix+f.name] = f.item->checkState(0) == Qt::Checked;
	}
	foreach(Directory d,dir.dirs)
	{
		generateMap(map,d,prefix+d.name+"/");
	}
}

bool TorrentOptsWidget::accept()
{
	return true;
}

////////////////////////////////////////////////////////////

void TorrentDownloadSettings::load()
{
	g_settings->beginGroup("bittorrent");
	spinListenStart->setValue(g_settings->value("listenstart", 6881).toInt());
	spinListenEnd->setValue(g_settings->value("listenend", 6889).toInt());
	spinRatio->setValue(g_settings->value("ratio", 1.0).toDouble());
	spinConnections->setValue(g_settings->value("connections").toInt());
	spinUploads->setValue(g_settings->value("uploads",4).toInt());
	g_settings->endGroup();
}

bool TorrentDownloadSettings::accept()
{
	if(spinListenStart->value() > spinListenEnd->value())
	{
		QMessageBox::critical(0, QObject::tr("Error"), QObject::tr("The specified port range is invalid."));
		return false;
	}
	else
		return true;
}

void TorrentDownloadSettings::accepted()
{
	g_settings->beginGroup("bittorrent");
	g_settings->setValue("listenstart", spinListenStart->value());
	g_settings->setValue("listenend", spinListenEnd->value());
	g_settings->setValue("ratio", spinRatio->value());
	g_settings->setValue("connections", spinConnections->value());
	g_settings->setValue("uploads", spinUploads->value());
	g_settings->endGroup();
	
	TorrentDownload::globalInit();
}

