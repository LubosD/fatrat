#include "fatrat.h"
#include "TorrentDownload.h"
#include "TorrentSettings.h"
#include "RuntimeException.h"
#include "GeneralDownload.h"
#include "TorrentPiecesModel.h"
#include "TorrentPeersModel.h"
#include "TorrentFilesModel.h"

#include <QIcon>
#include <QMenu>
#include <QTemporaryFile>
#include <QHeaderView>
#include <QMessageBox>
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <libtorrent/bencode.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/extensions/ut_pex.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <QLibrary>

extern QSettings* g_settings;

libtorrent::session* TorrentDownload::m_session = 0;
TorrentWorker* TorrentDownload::m_worker = 0;
bool TorrentDownload::m_bDHT = false;

void* g_pGeoIP = 0;
QLibrary g_geoIPLib;
void* (*GeoIP_new)(int);
const char* (*GeoIP_country_name_by_addr)(void*, const char*);
const char* (*GeoIP_country_code_by_addr)(void*, const char*);
void (*GeoIP_delete)(void*);

TorrentDownload::TorrentDownload()
	:  m_info(0), m_nPrevDownload(0), m_nPrevUpload(0), m_bHasHashCheck(false), m_pFileDownload(0)
{
	m_worker->addObject(this);
}

TorrentDownload::~TorrentDownload()
{
	m_worker->removeObject(this);
	if(m_handle.is_valid())
		m_session->remove_torrent(m_handle);
}

int TorrentDownload::acceptable(QString uri, bool)
{
	const bool istorrent = uri.endsWith(".torrent", Qt::CaseInsensitive);
        if(uri.startsWith("http://") || uri.startsWith("ftp://"))
                return (istorrent) ? 3 : 2;
	if(istorrent)
	{
		if(uri[0] == '/' || uri.startsWith("file://"))
			return 2;
	}
        return 0;
}

void TorrentDownload::globalInit()
{
	boost::filesystem::path::default_name_check(boost::filesystem::native);
	
	m_session = new libtorrent::session(libtorrent::fingerprint("FR", 0, 1, 0, 0));
	m_session->set_severity_level(libtorrent::alert::warning);
	
	applySettings();
	
	if(g_settings->value("torrent/pex", getSettingsDefault("torrent/pex")).toBool())
		m_session->add_extension(&libtorrent::create_ut_pex_plugin);
	
	m_worker = new TorrentWorker;
	
	g_geoIPLib.setFileName("libGeoIP");
	if(g_geoIPLib.load())
	{
		*((void**) &GeoIP_new) = g_geoIPLib.resolve("GeoIP_new");
		*((void**) &GeoIP_country_name_by_addr) = g_geoIPLib.resolve("GeoIP_country_name_by_addr");
		*((void**) &GeoIP_country_code_by_addr) = g_geoIPLib.resolve("GeoIP_country_code_by_addr");
		*((void**) &GeoIP_delete) = g_geoIPLib.resolve("GeoIP_delete");
		
		g_pGeoIP = GeoIP_new(1 /*GEOIP_MEMORY_CACHE*/);
	}
}

void TorrentDownload::applySettings()
{
	g_settings->beginGroup("torrent");
	
	int lstart,lend;
	libtorrent::session_settings settings;
	
	lstart = g_settings->value("listen_start", getSettingsDefault("torrent/listen_start")).toInt();
	lend = g_settings->value("listen_end", getSettingsDefault("torrent/listen_end")).toInt();
	
	if(lend < lstart)
		lend = lstart;
	
	if(m_session->listen_port() != lstart)
		m_session->listen_on(std::pair<int,int>(lstart,lend));
	
	if(g_settings->value("dht", getSettingsDefault("torrent/dht")).toBool())
	{
		try
		{
			m_session->start_dht(bdecode_simple(g_settings->value("dht_state").toByteArray()));
			m_session->add_dht_router(std::pair<std::string, int>("router.bittorrent.com", 6881));
			m_bDHT = true;
		}
		catch(...)
		{
			m_bDHT = false;
			qDebug() << "Failed to start DHT!";
		}
	}
	else
		m_session->stop_dht();
	
	//m_session->set_max_uploads(g_settings->value("maxuploads", getSettingsDefault("torrent/maxuploads")).toInt());
	//m_session->set_max_connections(g_settings->value("maxconnections", getSettingsDefault("torrent/maxconnections")).toInt());
	
	settings.file_pool_size = g_settings->value("maxfiles", getSettingsDefault("torrent/maxfiles")).toInt();
	settings.use_dht_as_fallback = false; // i.e. use DHT always
	settings.user_agent = "FatRat/" VERSION;
	settings.max_out_request_queue = 300;
	//settings.piece_timeout = 50;
	settings.request_queue_time = 30.f;
	settings.max_out_request_queue = 100;
	m_session->set_settings(settings);
	
	libtorrent::pe_settings ps;
	ps.in_enc_policy = ps.out_enc_policy = libtorrent::pe_settings::enabled;
	ps.allowed_enc_level = libtorrent::pe_settings::both;
	m_session->set_pe_settings(ps);
	
	g_settings->endGroup();
}

void TorrentDownload::globalExit()
{
	if(m_bDHT)
	{
		g_settings->beginGroup("bittorrent");
		g_settings->setValue("dht_state", bencode_simple(m_session->dht_state()));
		g_settings->endGroup();
	}
	
	delete m_worker;
	delete m_session;
	
	if(g_pGeoIP != 0)
		GeoIP_delete(g_pGeoIP);
}

QString TorrentDownload::name() const
{
	if(m_handle.is_valid())
	{
		std::string str = m_handle.name();
		return QString::fromUtf8(str.c_str());
	}
	else if(m_pFileDownload != 0)
		return tr("Downloading the .torrent file...");
	else
		return "*INVALID*";
}

void TorrentDownload::createDefaultPriorityList()
{
	int numFiles = m_info->num_files();
	m_vecPriorities.assign(numFiles, 1);
}

void TorrentDownload::init(QString source, QString target)
{
	m_strTarget = target;
	
	try
	{
		if(source.startsWith("file://"))
			source = source.mid(7);
		if(source.startsWith('/'))
		{
			QFile in(source);
			QByteArray data;
			const char* p;
			
			if(!in.open(QIODevice::ReadOnly))
				throw RuntimeException(tr("Unable to open the file!"));
			
			data = in.readAll();
			p = data.data();
			
			m_info = new libtorrent::torrent_info( libtorrent::bdecode(p, p+data.size()) );
			
			m_handle = m_session->add_torrent(boost::intrusive_ptr<libtorrent::torrent_info>(m_info), target.toStdString(), libtorrent::entry(), libtorrent::storage_mode_sparse, !isActive());
			
			m_handle.set_ratio(1.2f);
			m_handle.set_max_uploads(g_settings->value("maxuploads", getSettingsDefault("torrent/maxuploads")).toInt());
			m_handle.set_max_connections(g_settings->value("maxconnections", getSettingsDefault("torrent/maxconnections")).toInt());
			
			m_bHasHashCheck = true;
			
			createDefaultPriorityList();
		
			m_worker->doWork();
			storeTorrent(source);
		}
		else
		{
			QString error,name;
			GeneralDownload* download;
			QTemporaryFile* temp;
			QDir dir;
			
			m_pFileDownload = download = new GeneralDownload(true);
			temp = new QTemporaryFile(download);
			
			if(!temp->open())
			{
				m_pFileDownload = 0;
				delete download;
				throw RuntimeException(tr("Cannot create a temporary file"));
			}
			
			dir = temp->fileName();
			name = dir.dirName();
			qDebug() << "Downloading torrent to" << temp->fileName();
			dir.cdUp();
			
			connect(download, SIGNAL(stateChanged(Transfer::State,Transfer::State)), this, SLOT(fileStateChanged(Transfer::State,Transfer::State)));
			download->init(source, dir.path());
			download->setTargetName(name);
			download->setState(Active);
		}
	}
	catch(const libtorrent::invalid_encoding&)
	{
		throw RuntimeException(tr("The torrent file is invalid."));
	}
	catch(const std::exception& e)
	{
		throw RuntimeException(e.what());
	}
}

bool TorrentDownload::storeTorrent(QString orig)
{
	QString str = storedTorrentName();
	QDir dir = QDir::home();
	
	dir.mkpath(".local/share/fatrat/torrents");
	if(!dir.cd(".local/share/fatrat/torrents"))
		return false;
	
	str = dir.absoluteFilePath(str);
	
	if(str != orig)
		return QFile::copy(orig, str);
	else
		return true;
}

QString TorrentDownload::storedTorrentName()
{
	if(!m_info)
		return QString();
	
	QString hash;
	
	const libtorrent::big_number& bn = m_info->info_hash();
	hash = QByteArray((char*) bn.begin(), 20).toHex();
	
	return QString("%1 - %2.torrent").arg(name()).arg(hash);
}

void TorrentDownload::fileStateChanged(Transfer::State prev,Transfer::State now)
{
	if(prev != Active || !m_pFileDownload)
		return;
	
	if(now != Completed)
	{
		qDebug() << "Torrent-get completed without success";
		m_strError = tr("Failed to download the .torrent file");
		setState(Failed);
	}
	else
	{
		try
		{
			init(static_cast<GeneralDownload*>(m_pFileDownload)->filePath(), m_strTarget);
		}
		catch(const RuntimeException& e)
		{
			qDebug() << "Failed to load torrent:" << e.what();
			m_strError = e.what();
			setState(Failed);
		}
	}
	
	m_pFileDownload->deleteLater();
	m_pFileDownload = 0;
}

void TorrentDownload::setObject(QString target)
{
	if(m_handle.is_valid())
	{
		std::string newplace = target.toStdString();
		try
		{
			m_handle.move_storage(newplace);
		}
		catch(...)
		{
			throw RuntimeException(tr("Cannot change storage!"));
		}
	}
}

QString TorrentDownload::object() const
{
	return m_strTarget;
}

void TorrentDownload::changeActive(bool nowActive)
{
	if(m_handle.is_valid())
	{
		if(nowActive)
			m_handle.resume();
		else
			m_handle.pause();
	}
	else if(m_pFileDownload == 0)
	{
		qDebug() << "changeActive() and the handle is not valid";
		//setState(Failed);
	}
}

void TorrentDownload::setSpeedLimits(int down, int up)
{
	if(m_handle.is_valid())
	{
		if(!down)
			down--;
		if(!up)
			up--;
		m_handle.set_upload_limit(up);
		m_handle.set_download_limit(down);
		
		//qDebug() << "Limits are D:" << m_handle.download_limit() << "U:" << m_handle.upload_limit();
	}
	else
		qDebug() << "Warning: torrent speed limit was not set:" << down << up;
}

qulonglong TorrentDownload::done() const
{
	if(m_handle.is_valid())
		return m_status.total_wanted_done;
	else if(m_pFileDownload != 0)
		return m_pFileDownload->done();
	else
		return 0;
}

qulonglong TorrentDownload::total() const
{
	if(m_handle.is_valid())
		return m_status.total_wanted;
	else if(m_pFileDownload != 0)
		return m_pFileDownload->total();
	else
		return 0;
}

void TorrentDownload::speeds(int& down, int& up) const
{
	if(m_handle.is_valid())
	{
		libtorrent::torrent_status status = m_handle.status();
		down = status.download_payload_rate;
		up = status.upload_payload_rate;
	}
	else
		down = up = 0;
}

QByteArray TorrentDownload::bencode_simple(libtorrent::entry e)
{
	std::vector<char> buffer;
	libtorrent::bencode(std::back_inserter(buffer), e);
	return QByteArray(buffer.data(), buffer.size());
}

QString TorrentDownload::bencode(libtorrent::entry e)
{
	return bencode_simple(e).toBase64();
}

libtorrent::entry TorrentDownload::bdecode_simple(QByteArray array)
{
	if(array.isEmpty())
		return libtorrent::entry();
	else
		return libtorrent::bdecode(array.constData(), array.constData() + array.size());
}

libtorrent::entry TorrentDownload::bdecode(QString d)
{
	if(d.isEmpty())
		return libtorrent::entry();
	else
	{
		QByteArray array;
		array = QByteArray::fromBase64(d.toUtf8());
		return libtorrent::bdecode(array.constData(), array.constData() + array.size());
	}
}

void TorrentDownload::load(const QDomNode& map)
{
	Transfer::load(map);
	
	try
	{
		libtorrent::entry torrent_resume;
		QString str;
		QDir dir = QDir::home();
	
		dir.cd(".local/share/fatrat/torrents");
		
		m_strTarget = str = getXMLProperty(map, "target");
		
		QByteArray file = dir.absoluteFilePath( getXMLProperty(map, "torrent_file") ).toUtf8();
		std::ifstream in(file.data(), std::ios_base::binary);
		in.unsetf(std::ios_base::skipws);
		
		if(!in.is_open())
		{
			m_strError = tr("Unable to open the file!");
			setState(Failed);
			return;
		}
		
		m_info = new libtorrent::torrent_info( libtorrent::bdecode(std::istream_iterator<char>(in), std::istream_iterator<char>()) );
		
		torrent_resume = bdecode(getXMLProperty(map, "torrent_resume"));
		
		m_handle = m_session->add_torrent(boost::intrusive_ptr<libtorrent::torrent_info>( m_info ), str.toStdString(), torrent_resume, libtorrent::storage_mode_sparse, !isActive());
		
		m_handle.set_ratio(1.2f);
		m_handle.set_max_uploads(g_settings->value("maxuploads", getSettingsDefault("torrent/maxuploads")).toInt());
		m_handle.set_max_connections(g_settings->value("maxconnections", getSettingsDefault("torrent/maxconnections")).toInt());
		
		m_nPrevDownload = getXMLProperty(map, "downloaded").toLongLong();
		m_nPrevUpload = getXMLProperty(map, "uploaded").toLongLong();
		
		str = getXMLProperty(map, "priorities");
		
		if(str.isEmpty())
			createDefaultPriorityList();
		else
		{
			QStringList priorities = str.split('|');
			int numFiles = m_info->num_files();
			
			if(numFiles != priorities.size())
				createDefaultPriorityList();
			else
			{
				m_vecPriorities.resize(numFiles);
				
				for(int i=0;i<numFiles;i++)
					m_vecPriorities[i] = priorities[i].toInt();
			}
			
			m_handle.prioritize_files(m_vecPriorities);
		}
		
		std::vector<libtorrent::announce_entry> trackers;
		QDomElement n = map.firstChildElement("trackers");
		if(!n.isNull())
		{
			QDomElement tracker = n.firstChildElement("tracker");
			while(!tracker.isNull())
			{
				QByteArray url = tracker.firstChild().toText().data().toUtf8();
				trackers.push_back(libtorrent::announce_entry(url.data()));
				tracker = tracker.nextSiblingElement("tracker");
			}
			m_handle.replace_trackers(trackers);
		}
		
		m_worker->doWork();
	}
	catch(const std::exception& e)
	{
		m_strError = e.what();
		setState(Failed);
	}
}

void TorrentDownload::save(QDomDocument& doc, QDomNode& map)
{
	Transfer::save(doc, map);
	
	if(m_info != 0)
	{
		setXMLProperty(doc, map, "torrent_file", storedTorrentName());
		
		try
		{
			if(m_handle.is_valid())
				setXMLProperty(doc, map, "torrent_resume", bencode(m_handle.write_resume_data()));
		}
		catch(...) {}
	}
	
	setXMLProperty(doc, map, "target", object());
	setXMLProperty(doc, map, "downloaded", QString::number( totalDownload() ));
	setXMLProperty(doc, map, "uploaded", QString::number( totalUpload() ));
	
	QString prio;
	
	for(int i=0;i<m_vecPriorities.size();i++)
	{
		if(i > 0)
			prio += '|';
		prio += QString::number(m_vecPriorities[i]);
	}
	setXMLProperty(doc, map, "priorities", prio);
	
	if(m_handle.is_valid())
	{
		QDomElement sub = doc.createElement("trackers");
		std::vector<libtorrent::announce_entry> trackers = m_handle.trackers();
		for(size_t i=0;i<trackers.size();i++)
		{
			setXMLProperty(doc, sub, "tracker", QString::fromUtf8(trackers[i].url.c_str()));
		}
		
		sub = doc.createElement("url_seeds"); // TODO
	}
}

QString TorrentDownload::message() const
{
	QString state;
	
	if(this->state() == Failed)
		return m_strError;
	else if(m_pFileDownload != 0)
		return tr("Downloading .torrent file");
	
	if(!m_status.paused)
	{
		switch(m_status.state)
		{
		case libtorrent::torrent_status::queued_for_checking:
			state = tr("Queued for checking");
			break;
		case libtorrent::torrent_status::checking_files:
			state = tr("Checking files: %1%").arg(m_status.progress*100.f);
			break;
		case libtorrent::torrent_status::connecting_to_tracker:
			state = tr("Connecting to tracker");
			state += " | ";
		case libtorrent::torrent_status::seeding:
		case libtorrent::torrent_status::downloading:
			state += QString("Seeds: %1 (%2) | Peers: %3 (%4)");
			
			if(m_status.state == libtorrent::torrent_status::downloading)
				state = state.arg(m_status.num_seeds);
			else
				state = state.arg(QString());
			
			if(m_status.num_complete >= 0)
				state = state.arg(m_status.num_complete);
			else
				state = state.arg('?');
			state = state.arg(m_status.num_peers - m_status.num_seeds);
			
			if(m_status.num_incomplete >= 0)
				state = state.arg(m_status.num_incomplete);
			else
				state = state.arg('?');
			break;
		case libtorrent::torrent_status::finished:
			state = "Finished";
			break;
		case libtorrent::torrent_status::allocating:
			state = tr("Allocating: %1%").arg(m_status.progress*100.f);
			break;
		case libtorrent::torrent_status::downloading_metadata:
			state = "Downloading metadata";
			break;
		}
	}
	
	return state;
}

TorrentWorker::TorrentWorker()
{
	m_timer.start(1000);
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(doWork()));
}

void TorrentWorker::addObject(TorrentDownload* d)
{
	m_mutex.lock();
	m_objects << d;
	m_mutex.unlock();
}

void TorrentWorker::removeObject(TorrentDownload* d)
{
	m_mutex.lock();
	m_objects.removeAll(d);
	m_mutex.unlock();
}

TorrentDownload* TorrentWorker::getByHandle(libtorrent::torrent_handle handle) const
{
	foreach(TorrentDownload* d, m_objects)
	{
		if(d->m_handle == handle)
			return d;
	}
	return 0;
}

void TorrentWorker::doWork()
{
	m_mutex.lock();
	
	foreach(TorrentDownload* d, m_objects)
	{
		if(!d->m_handle.is_valid())
			continue;
		
		d->m_status = d->m_handle.status();
		
		if(d->m_bHasHashCheck && d->m_status.state != libtorrent::torrent_status::checking_files && d->m_status.state != libtorrent::torrent_status::queued_for_checking)
		{
			d->m_bHasHashCheck = false;
			d->m_nPrevDownload = d->done();
		}
		
		if(d->isActive())
		{
			if(d->mode() == Transfer::Download)
			{
				if(d->m_status.state == libtorrent::torrent_status::finished ||
				  d->m_status.state == libtorrent::torrent_status::seeding || d->m_handle.is_seed())
				{
					qDebug() << "According to the status, the torrent is complete";
					d->setMode(Transfer::Upload);
					d->logMessage(tr("Torrent has been downloaded"));
					d->m_handle.set_ratio(0);
				}
			}
			if(d->state() != Transfer::ForcedActive && d->mode() == Transfer::Upload)
			{
				double maxratio = g_settings->value("torrent/maxratio", getSettingsDefault("torrent/maxratio")).toDouble();
				if(double(d->totalUpload()) / d->totalDownload() > maxratio)
				{
					d->setState(Transfer::Completed);
					d->setMode(Transfer::Download);
				}
			}
		}
	}
	
	m_mutex.unlock();

#define IS_ALERT(type) libtorrent::type* alert = dynamic_cast<libtorrent::type*>(aaa)
	
	while(true)
	{
		libtorrent::alert* aaa;
		std::auto_ptr<libtorrent::alert> a = TorrentDownload::m_session->pop_alert();
		
		if((aaa = a.get()) == 0)
			break;
		
		qDebug() << "Libtorrent alert:" << QString(aaa->msg().c_str());
		
		if(IS_ALERT(torrent_alert))
		{
			TorrentDownload* d = getByHandle(alert->handle);
			const char* shit = static_cast<libtorrent::torrent_alert*>(a.get())->msg().c_str();
			QString errmsg = QString::fromUtf8(shit);
			
			if(!d)
				continue;
			
			if(IS_ALERT(file_error_alert))
			{
				d->setState(Transfer::Failed);
				d->m_strError = errmsg;
				d->logMessage(tr("File error: %1").arg(errmsg));
			}
			else if(IS_ALERT(tracker_announce_alert))
			{
				d->logMessage(tr("Tracker announce: %1").arg(errmsg));
			}
			else if(IS_ALERT(tracker_alert))
			{
				QString desc = tr("Tracker failure: %1, %2 times in a row ")
						.arg(errmsg)
						.arg(alert->times_in_row);
				
				if(alert->status_code != 0)
					desc += tr("(error %1)").arg(alert->status_code);
				else
					desc += tr("(timeout)");
				d->logMessage(desc);
			}
			else if(IS_ALERT(tracker_warning_alert))
			{
				d->logMessage(tr("Tracker warning: %1").arg(errmsg));
			}
			else if(IS_ALERT(fastresume_rejected_alert))
			{
				d->logMessage(tr("Fast-resume data have been rejected: %1").arg(errmsg));
			}
		}
		else if(IS_ALERT(peer_error_alert))
		{
			// TODO: what to do with this?
			std::string ip = alert->ip.address().to_string();
			qDebug() << "\tFor IP address" << ip.c_str() << alert->ip.port();
		}
	}
#undef IS_ALERT
}

WidgetHostChild* TorrentDownload::createSettingsWidget(QWidget* w,QIcon& i)
{
	i = QIcon(":/fatrat/bittorrent.png");
	return new TorrentSettings(w);
}

void TorrentDownload::forceReannounce()
{
	if(!m_handle.is_valid())
		return;
	
	if(m_status.state == libtorrent::torrent_status::seeding || m_status.state == libtorrent::torrent_status::downloading)
		m_handle.force_reannounce();
}

void TorrentDownload::fillContextMenu(QMenu& menu)
{
	QAction* a;
	
	a = menu.addAction(tr("Force announce"));
	connect(a, SIGNAL(triggered()), this, SLOT(forceReannounce()));
}

QObject* TorrentDownload::createDetailsWidget(QWidget* widget)
{
	return new TorrentDetails(widget, this);
}

WidgetHostChild* TorrentDownload::createOptionsWidget(QWidget* w)
{
	if(m_handle.is_valid())
		return new TorrentOptsWidget(w, this);
	else
		return 0;
}

void TorrentWorker::setDetailsObject(TorrentDetails* d)
{
	connect(&m_timer, SIGNAL(timeout()), d, SLOT(refresh()));
}

/////////////////////////////////////////////

TorrentDetails::TorrentDetails(QWidget* me, TorrentDownload* obj)
	: m_download(obj), m_bFilled(false)
{
	connect(obj, SIGNAL(destroyed(QObject*)), this, SLOT(destroy()));
	setupUi(me);
	TorrentDownload::m_worker->setDetailsObject(this);
	
	m_pPiecesModel = new TorrentPiecesModel(treePieces, obj);
	treePieces->setModel(m_pPiecesModel);
	treePieces->setItemDelegate(new BlockDelegate(treePieces));
	
	m_pPeersModel = new TorrentPeersModel(treePeers, obj);
	treePeers->setModel(m_pPeersModel);
	
	m_pFilesModel = new TorrentFilesModel(treeFiles, obj);
	treeFiles->setModel(m_pFilesModel);
	treeFiles->setItemDelegate(new TorrentProgressDelegate(treeFiles));
	
	QHeaderView* hdr = treePeers->header();
	hdr->resizeSection(1, 110);
	hdr->resizeSection(3, 80);
	
	for(int i=4;i<8;i++)
		hdr->resizeSection(i, 70);
	
	hdr->resizeSection(8, 300);
	
	hdr = treeFiles->header();
	hdr->resizeSection(0, 500);
	
	QAction* act;
	m_pMenuFiles = new QMenu( tr("Priority"), me );
	
	act = m_pMenuFiles->addAction( tr("Do not download") );
	connect(act, SIGNAL(triggered()), this, SLOT(setPriority0()));
	act = m_pMenuFiles->addAction( tr("Normal") );
	connect(act, SIGNAL(triggered()), this, SLOT(setPriority1()));
	act = m_pMenuFiles->addAction( tr("Increased") );
	connect(act, SIGNAL(triggered()), this, SLOT(setPriority4()));
	act = m_pMenuFiles->addAction( tr("Maximum") );
	connect(act, SIGNAL(triggered()), this, SLOT(setPriority7()));
	
	connect(treeFiles, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(fileContext(const QPoint&)));
	
	m_pMenuPeers = new QMenu(me);
	act = m_pMenuPeers->addAction( tr("Ban") );
	act = m_pMenuPeers->addAction( tr("Information") );
	connect(act, SIGNAL(triggered()), this, SLOT(peerInfo()));
	
	connect(treePeers, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(peerContext(const QPoint&)));
	
	refresh();
}

TorrentDetails::~TorrentDetails()
{
}

void TorrentDetails::setPriority(int p)
{
	foreach(int i, m_selFiles)
		m_download->m_vecPriorities[i] = p;
	m_download->m_handle.prioritize_files(m_download->m_vecPriorities);
}

void TorrentDetails::peerInfo()
{
	int row = treePeers->currentIndex().row();
	if(row != -1)
	{
		const libtorrent::peer_info& info = m_pPeersModel->m_peers[row];
		QMessageBox::information(treePeers, "FatRat", QString("Load balancing: %1").arg(info.load_balancing));
	}
}

void TorrentDetails::fileContext(const QPoint&)
{
	if(m_download && m_download->m_handle.is_valid())
	{
		int numFiles = m_download->m_info->num_files();
		QModelIndexList list = treeFiles->selectionModel()->selectedRows();
		
		m_selFiles.clear();
		
		foreach(QModelIndex in, list)
		{
			int row = in.row();
			
			if(row < numFiles)
				m_selFiles << row;
		}
		
		QMenu menu(treeFiles);
		menu.addMenu(m_pMenuFiles);
		menu.exec(QCursor::pos());
	}
}

void TorrentDetails::peerContext(const QPoint&)
{
	m_pMenuPeers->exec(QCursor::pos());
}

void TorrentDetails::destroy()
{
	m_download = 0;
}

void TorrentDetails::fill()
{
	if(m_download && m_download->m_handle.is_valid())
	{
		m_bFilled = true;
		
		boost::optional<boost::posix_time::ptime> time = m_download->m_info->creation_date();
		if(time)
		{
			std::string created = boost::posix_time::to_simple_string(time.get());
			labelCreationDate->setText(created.c_str());
		}
		labelPieceLength->setText( QString("%1 kB").arg(m_download->m_info->piece_length()/1024.f) );
		lineComment->setText(m_download->m_info->comment().c_str());
		lineCreator->setText(m_download->m_info->creator().c_str());
		labelPrivate->setText( m_download->m_info->priv() ? tr("yes") : tr("no"));
		
		m_pFilesModel->fill();
	}
}

void TorrentDetails::refresh()
{
	if(m_download && m_download->m_handle.is_valid())
	{
		if(!m_bFilled)
			fill();
		
		// GENERAL
		boost::posix_time::time_duration& next = m_download->m_status.next_announce;
		boost::posix_time::time_duration& intv = m_download->m_status.announce_interval;
		
		if(m_download->m_status.distributed_copies != -1)
			labelAvailability->setText(QString::number(m_download->m_status.distributed_copies));
		else
			labelAvailability->setText("-");
		labelTracker->setText(tr("%1 (refresh in %2:%3:%4, every %5:%6:%7)")
				.arg(m_download->m_status.current_tracker.c_str())
				.arg(next.hours()).arg(next.minutes(),2,10,QChar('0')).arg(next.seconds(),2,10,QChar('0'))
				.arg(intv.hours()).arg(intv.minutes(),2,10,QChar('0')).arg(intv.seconds(),2,10,QChar('0')));
		
		const std::vector<bool>* pieces = m_download->m_status.pieces;
		
		if(pieces != 0 && m_vecPieces != (*pieces))
		{
			widgetCompletition->generate(*pieces);
			m_vecPieces = *pieces;
			
			// FILES
			m_pFilesModel->refresh(&m_vecPieces);
		}
		
		// ratio
		qint64 d, u;
		QString ratio;
		
		d = m_download->totalDownload();
		u = m_download->totalUpload();
		
		if(!d)
			ratio = QString::fromUtf8("∞");
		else
			ratio = QString::number(double(u)/double(d));
		labelRatio->setText(ratio);
		
		labelTotalDownload->setText(formatSize(d));
		labelTotalUpload->setText(formatSize(u));
		
		// PIECES IN PROGRESS
		m_pPiecesModel->refresh();
		
		// CONNECTED PEERS
		m_pPeersModel->refresh();
	}
}

//////////////////////////////////////////////////////////////////

TorrentOptsWidget::TorrentOptsWidget(QWidget* me, TorrentDownload* parent)
	: m_download(parent), m_bUpdating(false)
{
	setupUi(me);
	
	QTreeWidgetItem* hdr = treeFiles->headerItem();
	hdr->setText(0, tr("Name"));
	hdr->setText(1, tr("Size"));
	
	connect(pushAddUrlSeed, SIGNAL(clicked()), this, SLOT(addUrlSeed()));
	connect(pushTrackerAdd, SIGNAL(clicked()), this, SLOT(addTracker()));
	connect(pushTrackerRemove, SIGNAL(clicked()), this, SLOT(removeTracker()));
	
	connect(treeFiles, SIGNAL(itemChanged(QTreeWidgetItem*,int)), this, SLOT(fileItemChanged(QTreeWidgetItem*,int)));
}

void TorrentOptsWidget::load()
{
	for(libtorrent::torrent_info::file_iterator it = m_download->m_info->begin_files();
		it != m_download->m_info->end_files();
		it++)
	{
		QStringList elems = QString::fromUtf8(it->path.string().c_str()).split('/');
		//QString name = elems.takeLast();
		
		QTreeWidgetItem* item = 0;
		
		for(int x=0;x<elems.size();x++)
		{
			if(item != 0)
			{
				bool bFound = false;
				for(int i=0;i<item->childCount();i++)
				{
					QTreeWidgetItem* c = item->child(i);
					if(c->text(0) == elems[x])
					{
						bFound = true;
						item = c;
					}
				}
				
				if(!bFound)
					item = new QTreeWidgetItem(item, QStringList( elems[x] ));
			}
			else
			{
				bool bFound = false;
				for(int i=0;i<treeFiles->topLevelItemCount();i++)
				{
					QTreeWidgetItem* c = treeFiles->topLevelItem(i);
					if(c->text(0) == elems[x])
					{
						bFound = true;
						item = c;
					}
				}
				
				if(!bFound)
					item = new QTreeWidgetItem(treeFiles, QStringList( elems[x] ));
			}
		}
		
		// fill in info
		item->setText(1, formatSize(it->size));
		item->setData(1, Qt::UserRole, qint64(it->size));
		m_files << item;
	}
	
	for(int i=0;i<m_files.size();i++)
	{
		bool download = true;
		if(i < m_download->m_vecPriorities.size())
			download = m_download->m_vecPriorities[i] >= 1;
		
		m_files[i]->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
		m_files[i]->setCheckState(0, download ? Qt::Checked : Qt::Unchecked);
	}
	
	recursiveUpdateDown(treeFiles->invisibleRootItem());
	treeFiles->expandAll();
	
	std::vector<std::string> seeds = m_download->m_info->url_seeds();
	for(int i=0;i<seeds.size();i++)
		listUrlSeeds->addItem(QString::fromUtf8(seeds[i].c_str()));
	
	m_trackers = m_download->m_handle.trackers();
	for(int i=0;i<m_trackers.size();i++)
		listTrackers->addItem(QString::fromUtf8(m_trackers[i].url.c_str()));
}

void TorrentOptsWidget::accepted()
{
	for(int i=0;i<m_files.size();i++)
	{
		bool yes = m_files[i]->checkState(0) == Qt::Checked;
		int& prio = m_download->m_vecPriorities[i];
		
		if(yes && !prio)
			prio = 1;
		else if(!yes && prio)
			prio = 0;
	}
	m_download->m_handle.prioritize_files(m_download->m_vecPriorities);
	
	foreach(QString url, m_seeds)
		m_download->m_handle.add_url_seed(url.toStdString());
	m_download->m_handle.replace_trackers(m_trackers);
}

void TorrentOptsWidget::addUrlSeed()
{
	QString url = lineUrl->text();
	if(url.startsWith("http://"))
	{
		m_seeds << url;
		listUrlSeeds->addItem(url);
		lineUrl->clear();
	}
}

void TorrentOptsWidget::addTracker()
{
	QString url = lineTracker->text();
	
	if(url.startsWith("http://") || url.startsWith("udp://"))
	{
		m_trackers.push_back( libtorrent::announce_entry ( url.toStdString() ) );
		listTrackers->addItem(url);
		lineTracker->clear();
	}
}

void TorrentOptsWidget::removeTracker()
{
	int cur = listTrackers->currentRow();
	if(cur != -1)
	{
		delete listTrackers->takeItem(cur);
		m_trackers.erase(m_trackers.begin() + cur);
	}
}

void TorrentOptsWidget::fileItemChanged(QTreeWidgetItem* item, int column)
{
	if(column != 0 || m_bUpdating)
		return;
	
	m_bUpdating = true;
	
	if(item->childCount())
	{
		// directory
		recursiveCheck(item, item->checkState(0));
	}
	
	if(QTreeWidgetItem* parent = item->parent())
		recursiveUpdate(parent);
	
	m_bUpdating = false;
}

void TorrentOptsWidget::recursiveUpdate(QTreeWidgetItem* item)
{
	int yes = 0, no = 0;
	int total = item->childCount();
	
	for(int i=0;i<total;i++)
	{
		int state = item->child(i)->checkState(0);
		if(state == Qt::Checked)
			yes++;
		else if(state == Qt::Unchecked)
			no++;
	}
	
	if(yes == total)
		item->setCheckState(0, Qt::Checked);
	else if(no == total)
		item->setCheckState(0, Qt::Unchecked);
	else
		item->setCheckState(0, Qt::PartiallyChecked);
	
	if(QTreeWidgetItem* parent = item->parent())
		recursiveUpdate(parent);
}

qint64 TorrentOptsWidget::recursiveUpdateDown(QTreeWidgetItem* item)
{
	int yes = 0, no = 0;
	int total = item->childCount();
	qint64 size = 0;
	
	for(int i=0;i<total;i++)
	{
		QTreeWidgetItem* child = item->child(i);
		
		if(child->childCount())
			size += recursiveUpdateDown(child);
		
		int state = child->checkState(0);
		if(state == Qt::Checked)
			yes++;
		else if(state == Qt::Unchecked)
			no++;
		
		size += child->data(1, Qt::UserRole).toLongLong();
	}
	
	if(yes == total)
		item->setCheckState(0, Qt::Checked);
	else if(no == total)
		item->setCheckState(0, Qt::Unchecked);
	else
		item->setCheckState(0, Qt::PartiallyChecked);
	
	item->setText(1, formatSize(size));
	
	return size;
}

void TorrentOptsWidget::recursiveCheck(QTreeWidgetItem* item, Qt::CheckState state)
{
	item->setCheckState(0, state);
	for(int i=0;i<item->childCount();i++)
		recursiveCheck(item->child(i), state);
}

