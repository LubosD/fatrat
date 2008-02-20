#include "fatrat.h"
#include "Logger.h"
#include "TorrentDownload.h"
#include "TorrentSettings.h"
#include "RuntimeException.h"
#include "TorrentPiecesModel.h"
#include "TorrentPeersModel.h"
#include "TorrentFilesModel.h"

#include <libtorrent/bencode.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/extensions/ut_pex.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <fstream>
#include <stdexcept>
#include <iostream>

#include <QIcon>
#include <QMenu>
#include <QHeaderView>
#include <QMessageBox>
#include <QLibrary>
#include <QDir>
#include <QUrl>
#include <QProcess>
#include <QtDebug>

extern QSettings* g_settings;

libtorrent::session* TorrentDownload::m_session = 0;
TorrentWorker* TorrentDownload::m_worker = 0;
bool TorrentDownload::m_bDHT = false;

static const char* TORRENT_FILE_STORAGE = ".local/share/fatrat/torrents";

void* g_pGeoIP = 0;
QLibrary g_geoIPLib;
void* (*GeoIP_new)(int);
const char* (*GeoIP_country_name_by_addr)(void*, const char*);
const char* (*GeoIP_country_code_by_addr)(void*, const char*);
void (*GeoIP_delete)(void*);

TorrentDownload::TorrentDownload()
	:  m_info(0), m_nPrevDownload(0), m_nPrevUpload(0), m_bHasHashCheck(false), m_pFileDownload(0)
		, m_pFileDownloadTemp(0)
{
	m_worker->addObject(this);
}

TorrentDownload::~TorrentDownload()
{
	m_worker->removeObject(this);
	if(m_handle.is_valid())
		m_session->remove_torrent(m_handle);
	//delete m_info;
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
		GeoIP_new = (void*(*)(int)) g_geoIPLib.resolve("GeoIP_new");
		GeoIP_country_name_by_addr = (const char*(*)(void*, const char*)) g_geoIPLib.resolve("GeoIP_country_name_by_addr");
		GeoIP_country_code_by_addr = (const char*(*)(void*, const char*)) g_geoIPLib.resolve("GeoIP_country_code_by_addr");
		GeoIP_delete = (void(*)(void*)) g_geoIPLib.resolve("GeoIP_delete");
		
		g_pGeoIP = GeoIP_new(1 /*GEOIP_MEMORY_CACHE*/);
	}
}

void TorrentDownload::applySettings()
{
	int lstart,lend;
	libtorrent::session_settings settings;
	
	lstart = g_settings->value("torrent/listen_start", getSettingsDefault("torrent/listen_start")).toInt();
	lend = g_settings->value("torrent/listen_end", getSettingsDefault("torrent/listen_end")).toInt();
	
	if(lend < lstart)
		lend = lstart;
	
	if(m_session->listen_port() != lstart)
		m_session->listen_on(std::pair<int,int>(lstart,lend));
	
	if(g_settings->value("torrent/dht", getSettingsDefault("torrent/dht")).toBool())
	{
		try
		{
			m_session->start_dht(bdecode_simple(g_settings->value("torrent/dht_state").toByteArray()));
			m_session->add_dht_router(std::pair<std::string, int>("router.bittorrent.com", 6881));
			m_bDHT = true;
		}
		catch(...)
		{
			m_bDHT = false;
			Logger::global()->enterLogMessage("BitTorrent", tr("Failed to start DHT!"));
		}
	}
	else
		m_session->stop_dht();
	
	m_session->set_max_uploads(g_settings->value("torrent/maxuploads", getSettingsDefault("torrent/maxuploads")).toInt());
	m_session->set_max_connections(g_settings->value("torrent/maxconnections", getSettingsDefault("torrent/maxconnections")).toInt());
	
	settings.file_pool_size = g_settings->value("torrent/maxfiles", getSettingsDefault("torrent/maxfiles")).toInt();
	settings.use_dht_as_fallback = false; // i.e. use DHT always
	settings.user_agent = "FatRat " VERSION;
	//settings.max_out_request_queue = 300;
	//settings.piece_timeout = 50;
	settings.max_failcount = 7;
	settings.request_queue_time = 30.f;
	settings.max_out_request_queue = 100;
	
	QByteArray external_ip = g_settings->value("torrent/external_ip").toString().toUtf8();
	if(!external_ip.isEmpty())
		settings.announce_ip = asio::ip::address::from_string(external_ip.constData());
	
	m_session->set_settings(settings);
	
	libtorrent::pe_settings ps;
	ps.in_enc_policy = ps.out_enc_policy = libtorrent::pe_settings::enabled;
	ps.allowed_enc_level = libtorrent::pe_settings::both;
	m_session->set_pe_settings(ps);
	
	// Proxy settings
	QUuid tracker, seed, peer;
	
	tracker = g_settings->value("torrent/proxy_tracker").toString();
	seed = g_settings->value("torrent/proxy_webseed").toString();
	peer = g_settings->value("torrent/proxy_peer").toString();
	
	libtorrent::proxy_settings proxy;
	
	proxy = proxyToLibtorrent(Proxy::getProxy(tracker));
	m_session->set_tracker_proxy(proxy);
	
	proxy = proxyToLibtorrent(Proxy::getProxy(seed));
	m_session->set_web_seed_proxy(proxy);
	
	proxy = proxyToLibtorrent(Proxy::getProxy(peer));
	m_session->set_peer_proxy(proxy);
}

libtorrent::proxy_settings TorrentDownload::proxyToLibtorrent(Proxy p)
{
	libtorrent::proxy_settings s;
	
	if(p.nType != Proxy::ProxyNone)
	{
		s.hostname = p.strIP.toStdString();
		s.port = p.nPort;
		
		bool bAuth = !p.strUser.isEmpty();
		if(bAuth)
		{
			s.username = p.strUser.toStdString();
			s.password = p.strPassword.toStdString();
		}
		
		if(p.nType == Proxy::ProxyHttp)
		{
			if(bAuth)
				s.type = libtorrent::proxy_settings::http_pw;
			else
				s.type = libtorrent::proxy_settings::http;
		}
		else if(p.nType == Proxy::ProxySocks5)
		{
			if(bAuth)
				s.type = libtorrent::proxy_settings::socks5_pw;
			else
				s.type = libtorrent::proxy_settings::socks5;
		}
	}
	
	return s;
}

void TorrentDownload::globalExit()
{
	if(m_bDHT)
	{
		g_settings->setValue("torrent/dht_state", bencode_simple(m_session->dht_state()));
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

QString TorrentDownload::dataPath(bool bDirect) const
{
	if(m_handle.is_valid())
		return Transfer::dataPath(bDirect);
	else
		return QString();
}

void TorrentDownload::createDefaultPriorityList()
{
	int numFiles = m_info->num_files();
	m_vecPriorities.assign(numFiles, 1);
}

void TorrentDownload::init(QString source, QString target)
{
	m_strTarget = target;
	
	m_seedLimitRatio = g_settings->value("torrent/maxratio", getSettingsDefault("torrent/maxratio")).toDouble();
	m_seedLimitUpload = 0;
	
	try
	{
		if(source.startsWith("file://"))
			source = source.mid(7);
		if(source.startsWith('/'))
		{
			QFile in(source);
			QByteArray data;
			const char* p;
			libtorrent::storage_mode_t storageMode;
			
			if(!in.open(QIODevice::ReadOnly))
				throw RuntimeException(tr("Unable to open the file!"));
			
			data = in.readAll();
			p = data.data();
			
			m_info = new libtorrent::torrent_info( libtorrent::bdecode(p, p+data.size()) );
			
			storageMode = (libtorrent::storage_mode_t) g_settings->value("torrent/allocation", getSettingsDefault("torrent/allocation")).toInt();
			m_handle = m_session->add_torrent(boost::intrusive_ptr<libtorrent::torrent_info>(m_info), target.toStdString(), libtorrent::entry(), storageMode, !isActive());
			
			int limit;
			
			limit = g_settings->value("torrent/maxuploads_loc", getSettingsDefault("torrent/maxuploads_loc")).toInt();
			m_handle.set_max_uploads(limit ? limit : limit-1);
			
			limit = g_settings->value("torrent/maxconnections_loc", getSettingsDefault("torrent/maxconnections_loc")).toInt();
			m_handle.set_max_connections(limit ? limit : limit-1);
			
			m_bHasHashCheck = true;
			
			m_handle.set_ratio(1.2f);
			createDefaultPriorityList();
		
			m_worker->doWork();
			storeTorrent(source);
		}
		else
		{
			QString error,name;
			QDir dir;
			
			m_pFileDownload = new QHttp(this);
			m_pFileDownloadTemp = new QTemporaryFile(m_pFileDownload);
			
			if(!m_pFileDownloadTemp->open())
			{
				delete m_pFileDownload;
				m_pFileDownload = 0;
				throw RuntimeException(tr("Cannot create a temporary file"));
			}
			
			dir = m_pFileDownloadTemp->fileName();
			name = dir.dirName();
			qDebug() << "Downloading torrent to" << m_pFileDownloadTemp->fileName();
			dir.cdUp();
			
			QUrl url(source);
			m_pFileDownload->setHost(url.host(), url.port(80));
			connect(m_pFileDownload, SIGNAL(done(bool)), this, SLOT(torrentFileDone(bool)));
			
			if(url.hasQuery())
				m_pFileDownload->get(url.path() + '?' + url.encodedQuery(), m_pFileDownloadTemp);
			else
				m_pFileDownload->get(url.path(), m_pFileDownloadTemp);
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
	
	dir.mkpath(TORRENT_FILE_STORAGE);
	if(!dir.cd(TORRENT_FILE_STORAGE))
		return false;
	
	str = dir.absoluteFilePath(str);
	
	if(str != orig)
		return QFile::copy(orig, str);
	else
		return true;
}

QString TorrentDownload::storedTorrentName() const
{
	if(!m_info)
		return QString();
	
	QString hash;
	
	const libtorrent::big_number& bn = m_info->info_hash();
	hash = QByteArray((char*) bn.begin(), 20).toHex();
	
	return QString("%1 - %2.torrent").arg(name()).arg(hash);
}

void TorrentDownload::torrentFileDone(bool error)
{
	if(error)
	{
		m_strError = tr("Failed to download the .torrent file");
		setState(Failed);
	}
	else
	{
		try
		{
			m_pFileDownloadTemp->flush();
			init(m_pFileDownloadTemp->fileName(), m_strTarget);
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
	m_pFileDownloadTemp = 0;
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
	bool bEnableRecheck = false;
	
	if(m_handle.is_valid())
	{
		if(nowActive)
			m_handle.resume();
		else
		{
			bEnableRecheck = true;
			m_handle.pause();
		}
	}
	/*else if(m_pFileDownload == 0)
	{
		qDebug() << "changeActive() and the handle is not valid";
		//setState(Failed);
	}*/
}

void TorrentDownload::setSpeedLimits(int down, int up)
{
	if(m_handle.is_valid())
	{
		if(!down)
			down--;
		if(!up)
			up--;
		qDebug() << name() << ":" << "D:" << down << "U:" << up;
		m_handle.set_upload_limit(up);
		m_handle.set_download_limit(down);
	}
}

qulonglong TorrentDownload::done() const
{
	if(m_handle.is_valid())
		return qMax<qint64>(0, m_status.total_wanted_done);
	else
		return 0;
}

qulonglong TorrentDownload::total() const
{
	if(m_handle.is_valid())
		return m_status.total_wanted;
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
	
		dir.cd(TORRENT_FILE_STORAGE);
		
		str = getXMLProperty(map, "seedlimitratio");
		if(!str.isEmpty())
			m_seedLimitRatio = str.toDouble();
		else
			m_seedLimitRatio = g_settings->value("torrent/maxratio", getSettingsDefault("torrent/maxratio")).toDouble();
		
		str = getXMLProperty(map, "seedlimitupload");
		m_seedLimitUpload = str.toInt();
		
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
		
		libtorrent::storage_mode_t storageMode;
		
		storageMode = (libtorrent::storage_mode_t) g_settings->value("torrent/allocation", getSettingsDefault("torrent/allocation")).toInt();
		
		m_handle = m_session->add_torrent(boost::intrusive_ptr<libtorrent::torrent_info>( m_info ), str.toStdString(), torrent_resume, storageMode, true);
		
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
		
		std::set<std::string> cur_seeds = m_handle.url_seeds();
		std::set<std::string> now_seeds;
		
		n = map.firstChildElement("url_seeds");
		if(!n.isNull())
		{
			QDomElement seed = n.firstChildElement("url");
			while(!seed.isNull())
			{
				QByteArray url = seed.firstChild().toText().data().toUtf8();
				now_seeds.insert(url.constData());
				seed = seed.nextSiblingElement("url");
			}
			
			std::set<std::string> diff;
			
			std::set_difference(cur_seeds.begin(), cur_seeds.end(), now_seeds.begin(), now_seeds.end(), std::inserter(diff, diff.begin()));
			for(std::set<std::string>::iterator it=diff.begin(); it != diff.end(); it++)
				m_handle.remove_url_seed(*it);
			
			diff.clear();
			
			std::set_difference(now_seeds.begin(), now_seeds.end(), cur_seeds.begin(), cur_seeds.end(), std::inserter(diff, diff.begin()));
			for(std::set<std::string>::iterator it=diff.begin(); it != diff.end(); it++)
				m_handle.add_url_seed(*it);
		}
		
		if(isActive())
			m_handle.resume();
		
		m_worker->doWork();
	}
	catch(const std::exception& e)
	{
		m_strError = e.what();
		setState(Failed);
	}
}

void TorrentDownload::save(QDomDocument& doc, QDomNode& map) const
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
	
	setXMLProperty(doc, map, "seedlimitratio", QString::number(m_seedLimitRatio));
	setXMLProperty(doc, map, "seedlimitupload", QString::number(m_seedLimitUpload));
	
	QString prio;
	
	for(size_t i=0;i<m_vecPriorities.size();i++)
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
		
		sub = doc.createElement("url_seeds");
		std::set<std::string> seeds = m_handle.url_seeds();
		for(std::set<std::string>::iterator it=seeds.begin(); it != seeds.end(); it++)
		{
			setXMLProperty(doc, sub, "url", QString::fromUtf8(it->c_str()));
		}
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
		case libtorrent::torrent_status::finished:
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
					d->setMode(Transfer::Upload);
					d->enterLogMessage(tr("Torrent has been downloaded"));
					d->m_handle.set_ratio(0);
				}
				else if(d->m_status.total_wanted == d->m_status.total_wanted_done)
				{
					d->enterLogMessage(tr("Requested parts of the torrent have been downloaded"));
					d->setMode(Transfer::Upload);
					d->m_handle.set_ratio(0);
				}
			}
			if(d->mode() == Transfer::Upload)
			{
				if(d->m_status.total_wanted < d->m_status.total_wanted_done)
					d->setMode(Transfer::Download);
				else if(d->state() != Transfer::ForcedActive)
				{
					if(double(d->totalUpload()) / d->totalDownload() >= d->m_seedLimitRatio
					  || (d->totalUpload() >= d->m_seedLimitUpload*1024*1024 && d->m_seedLimitUpload))
					{
						d->setState(Transfer::Completed);
						d->setMode(Transfer::Download);
					}
				}
			}
			
			int down, up, sdown, sup;
			
			down = d->m_handle.download_limit();
			up = d->m_handle.upload_limit();
			
			d->internalSpeedLimits(sdown, sup);
			
			if(!sdown) sdown--;
			if(!sup) sup--;
			
			if(down != sdown || up != sup)
				d->setSpeedLimits(sdown, sup);
		}
	}
	
	m_mutex.unlock();

#define IS_ALERT(type) libtorrent::type* alert = dynamic_cast<libtorrent::type*>(aaa)
#define IS_ALERT_S(type) dynamic_cast<libtorrent::type*>(aaa) != 0
	
	while(true)
	{
		libtorrent::alert* aaa;
		std::auto_ptr<libtorrent::alert> a = TorrentDownload::m_session->pop_alert();
		
		if((aaa = a.get()) == 0)
			break;
		
		Logger::global()->enterLogMessage("BitTorrent", tr("Alert: %1").arg(aaa->msg().c_str()));
		
		if(IS_ALERT(torrent_alert))
		{
			TorrentDownload* d = getByHandle(alert->handle);
			const char* shit = static_cast<libtorrent::torrent_alert*>(a.get())->msg().c_str();
			QString errmsg = QString::fromUtf8(shit);
			
			if(!d)
				continue;
			
			if(IS_ALERT_S(file_error_alert))
			{
				d->setState(Transfer::Failed);
				d->m_strError = errmsg;
				d->enterLogMessage(tr("File error: %1").arg(errmsg));
			}
			else if(IS_ALERT_S(tracker_announce_alert))
			{
				d->enterLogMessage(tr("Tracker announce: %1").arg(errmsg));
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
				d->enterLogMessage(desc);
			}
			else if(IS_ALERT_S(tracker_warning_alert))
			{
				d->enterLogMessage(tr("Tracker warning: %1").arg(errmsg));
			}
			else if(IS_ALERT_S(fastresume_rejected_alert))
			{
				d->enterLogMessage(tr("Fast-resume data have been rejected: %1").arg(errmsg));
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
#undef IS_ALERT_S
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
	else
		m_handle.scrape_tracker();
}

void TorrentDownload::forceRecheck()
{
	if(isActive() || !m_handle.is_valid())
		return;
	
	m_bHasHashCheck = false;
	
	// dirty but simple
	QDomDocument doc;
	QDomElement root = doc.createElement("fake");
	doc.appendChild(root);
	
	save(doc, root);
	
	int up = m_handle.upload_limit();
	int down = m_handle.download_limit();
	
	m_session->remove_torrent(m_handle);
	
	QDomNodeList list = root.childNodes();
	for(int i=0;i<list.size();i++)
	{
		if(list.at(i).nodeName() == "torrent_resume")
		{
			root.removeChild(list.at(i));
			break;
		}
	}
	
	load(root);
	setSpeedLimits(down, up);
}

void TorrentDownload::fillContextMenu(QMenu& menu)
{
	QAction* a;
	
	a = menu.addAction(tr("Force announce"));
	connect(a, SIGNAL(triggered()), this, SLOT(forceReannounce()));
	a = menu.addAction(QIcon(":/menu/reload.png"), tr("Recheck files"));
	a->setDisabled(isActive());
	connect(a, SIGNAL(triggered()), this, SLOT(forceRecheck()));
}

QObject* TorrentDownload::createDetailsWidget(QWidget* widget)
{
	return new TorrentDetails(widget, this);
}

WidgetHostChild* TorrentDownload::createOptionsWidget(QWidget* w)
{
	return new TorrentOptsWidget(w, this);
}

void TorrentWorker::setDetailsObject(TorrentDetails* d)
{
	connect(&m_timer, SIGNAL(timeout()), d, SLOT(refresh()));
}

/////////////////////////////////////////////

TorrentDetails::TorrentDetails(QWidget* me, TorrentDownload* obj)
	: m_download(obj), m_bFilled(false)
{
	connect(obj, SIGNAL(destroyed(QObject*)), this, SLOT(deleteLater()));
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
	hdr->resizeSection(3, 50);
	hdr->resizeSection(4, 70);
	
	for(int i=5;i<9;i++)
		hdr->resizeSection(i, 70);
	
	hdr->resizeSection(9, 300);
	
	hdr = treeFiles->header();
	hdr->resizeSection(0, 500);
	
	QAction* act;
	QMenu* submenu;
	
	m_pMenuFiles = new QMenu(me);
	submenu = new QMenu(tr("Priority"), m_pMenuFiles);
	
	act = submenu->addAction( tr("Do not download") );
	connect(act, SIGNAL(triggered()), this, SLOT(setPriority0()));
	act = submenu->addAction( tr("Normal") );
	connect(act, SIGNAL(triggered()), this, SLOT(setPriority1()));
	act = submenu->addAction( tr("Increased") );
	connect(act, SIGNAL(triggered()), this, SLOT(setPriority4()));
	act = submenu->addAction( tr("Maximum") );
	connect(act, SIGNAL(triggered()), this, SLOT(setPriority7()));
	
	m_pMenuFiles->addAction(actionOpenFile);
	m_pMenuFiles->addMenu(submenu);
	
	connect(treeFiles, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(fileContext(const QPoint&)));
	connect(treeFiles, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(openFile()));
	
	m_pMenuPeers = new QMenu(me);
	act = m_pMenuPeers->addAction( tr("Ban") );
	act = m_pMenuPeers->addAction( tr("Information") );
	connect(act, SIGNAL(triggered()), this, SLOT(peerInfo()));
	
	connect(treePeers, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(peerContext(const QPoint&)));
	connect(actionOpenFile, SIGNAL(triggered()), this, SLOT(openFile()));
	
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

void TorrentDetails::openFile()
{
	if(m_selFiles.size() != 1)
		return;
	
	int i = m_selFiles[0];
	
	QString relative = m_download->m_info->file_at(i).path.string().c_str();
	QString path = m_download->dataPath(false) + relative;
	
	QString command = QString("%1 \"%2\"")
			.arg(g_settings->value("fileexec", getSettingsDefault("fileexec")).toString())
			.arg(path);
	QProcess::startDetached(command);
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
		
		actionOpenFile->setEnabled(list.size() == 1);
		m_pMenuFiles->exec(QCursor::pos());
	}
}

void TorrentDetails::peerContext(const QPoint&)
{
	//m_pMenuPeers->exec(QCursor::pos());
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
		
		QString comment = m_download->m_info->comment().c_str();
		comment.replace('\n', "<br>");
		textComment->setHtml(comment);
		
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
		
		std::vector<int> avail;
		m_download->m_handle.piece_availability(avail);
		widgetAvailability->generate(avail);
		
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

void TorrentOptsWidget::startInvalid()
{
	stackedWidget->setCurrentIndex(0);
	m_timer.start(1000);
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(handleInvalid()));
	handleInvalid();
}

void TorrentOptsWidget::handleInvalid()
{
	if(m_download->m_handle.is_valid())
	{
		m_timer.stop();
		load();
		stackedWidget->setCurrentIndex(1);
	}
	else
	{
		if(m_download->state() == Transfer::Failed)
		{
			labelStatus->setText(tr("The .torrent file cannot be downloaded or is invalid."));
		}
		else
		{
			labelStatus->setText(tr("The .torrent is being downloaded, please wait."));
		}
	}
}

void TorrentOptsWidget::load()
{
	if(!m_download->m_handle.is_valid())
	{
		startInvalid();
		return;
	}
	
	doubleSeed->setValue(m_download->m_seedLimitRatio);
	checkSeedRatio->setChecked(m_download->m_seedLimitRatio > 0.0);
	
	lineSeed->setText(QString::number(m_download->m_seedLimitUpload));
	checkSeedUpload->setChecked(m_download->m_seedLimitUpload > 0);
	
	QHeaderView* hdr = treeFiles->header();
	hdr->resizeSection(0, 350);
	
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
		if(size_t(i) < m_download->m_vecPriorities.size())
			download = m_download->m_vecPriorities[i] >= 1;
		
		m_files[i]->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
		m_files[i]->setCheckState(0, download ? Qt::Checked : Qt::Unchecked);
	}
	
	recursiveUpdateDown(treeFiles->invisibleRootItem());
	treeFiles->expandAll();
	
	std::set<std::string> seeds = m_download->m_handle.url_seeds();
	for(std::set<std::string>::iterator it=seeds.begin(); it != seeds.end(); it++)
		listUrlSeeds->addItem(QString::fromUtf8(it->c_str()));
	
	m_trackers = m_download->m_handle.trackers();
	for(size_t i=0;i<m_trackers.size();i++)
		listTrackers->addItem(QString::fromUtf8(m_trackers[i].url.c_str()));
}

void TorrentOptsWidget::accepted()
{
	if(!m_download->m_handle.is_valid())
		return;
	
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
	
	std::vector<std::string> seeds = m_download->m_info->url_seeds();
	foreach(QString url, m_seeds)
	{
		if(std::find(seeds.begin(), seeds.end(), url.toStdString()) == seeds.end())
			m_download->m_handle.add_url_seed(url.toStdString());
	}
	for(size_t i=0;i<seeds.size();i++)
	{
		QString url = QString::fromUtf8(seeds[i].c_str());
		if(!m_seeds.contains(url))
			m_download->m_handle.remove_url_seed(seeds[i]);
	}
	
	m_download->m_handle.replace_trackers(m_trackers);
	
	m_download->m_seedLimitRatio = (checkSeedRatio->isChecked()) ? doubleSeed->value() : 0.0;
	m_download->m_seedLimitUpload = (checkSeedUpload->isChecked()) ? lineSeed->text().toInt() : 0;
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

