#include "fatrat.h"
#include "config.h"
#include "Logger.h"
#include "TorrentDownload.h"
#include "TorrentSettings.h"
#include "TorrentDetails.h"
#include "TorrentOptsWidget.h"
#include "RuntimeException.h"
#include "rss/RssFetcher.h"

#include <libtorrent/bencode.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/extensions/ut_pex.hpp>

#include <fstream>
#include <stdexcept>

#include <QIcon>
#include <QMenu>
#include <QLibrary>
#include <QDir>
#include <QUrl>
#include <QFile>
#include <QVector>
#include <QtDebug>

extern QSettings* g_settings;

libtorrent::session* TorrentDownload::m_session = 0;
TorrentWorker* TorrentDownload::m_worker = 0;
bool TorrentDownload::m_bDHT = false;
QList<QRegExp> TorrentDownload::m_listBTLinks;

const char* TORRENT_FILE_STORAGE = ".local/share/fatrat/torrents";

void* g_pGeoIP = 0;
QLibrary g_geoIPLib;
void* (*GeoIP_new)(int);
const char* (*GeoIP_country_name_by_addr)(void*, const char*);
const char* (*GeoIP_country_code_by_addr)(void*, const char*);
void (*GeoIP_delete)(void*);

TorrentDownload::TorrentDownload(bool bAuto)
	:  m_info(0), m_nPrevDownload(0), m_nPrevUpload(0), m_bHasHashCheck(false), m_bAuto(bAuto), m_pFileDownload(0)
		, m_pFileDownloadTemp(0), m_totalWanted(1), m_totalWantedDone(0)
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
	bool istorrent;
	
	if(uri.endsWith(".torrent", Qt::CaseInsensitive))
		istorrent = true;
	else
	{
		istorrent = false;
		
		for(int i=0;i<m_listBTLinks.size();i++)
		{
			if(m_listBTLinks[i].exactMatch(uri))
			{
				istorrent = true;
				break;
			}
			else
				qDebug() << m_listBTLinks[i].pattern() << "not matched by" << uri;
		}
	}
		
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
	
	QFile file;
	if(openDataFile(&file, "/data/btlinks.txt"))
	{
		while(!file.atEnd())
		{
			QByteArray line = file.readLine().trimmed();
			if(line.isEmpty())
				continue;
			m_listBTLinks << QRegExp(line);
		}
	}
}

void TorrentDownload::applySettings()
{
	static bool bUPnPActive = false, bNATPMPActive = false, bLSDActive = false;
	bool bUPnP, bNATPMP, bLSD;
	int lstart,lend;
	libtorrent::session_settings settings;
	
	lstart = getSettingsValue("torrent/listen_start").toInt();
	lend = getSettingsValue("torrent/listen_end").toInt();
	
	if(lend < lstart)
		lend = lstart;
	
	if(m_session->listen_port() != lstart)
		m_session->listen_on(std::pair<int,int>(lstart,lend));
	
	bUPnP = getSettingsValue("torrent/mapping_upnp").toBool();
	bNATPMP = getSettingsValue("torrent/mapping_natpmp").toBool();
	bLSD = getSettingsValue("torrent/mapping_lsd").toBool();
	
	if(bUPnP != bUPnPActive)
	{
		if(bUPnP)
			m_session->start_upnp();
		else
			m_session->stop_upnp();
		bUPnPActive = bUPnP;
	}
	if(bNATPMP != bNATPMPActive)
	{
		if(bNATPMP)
			m_session->start_natpmp();
		else
			m_session->stop_natpmp();
		bNATPMPActive = bNATPMP;
	}
	if(bLSD != bLSDActive)
	{
		if(bLSD)
			m_session->start_lsd();
		else
			m_session->stop_lsd();
		bLSDActive = bLSD;
	}
	
	if(getSettingsValue("torrent/dht").toBool())
	{
		QByteArray state = g_settings->value("torrent/dht_state").toByteArray();
		while(!m_bDHT)
		{
			try
			{
				m_session->start_dht(bdecode_simple(state));
				m_session->add_dht_router(std::pair<std::string, int>("router.bittorrent.com", 6881));
				m_bDHT = true;
				
				Logger::global()->enterLogMessage("BitTorrent", tr("DHT started"));
			}
			catch(const std::exception& e)
			{
				m_bDHT = false;
				Logger::global()->enterLogMessage("BitTorrent", tr("Failed to start DHT!") + ' ' + e.what());
				
				if(!state.isEmpty())
				{
					state.clear();
					continue;
				}
				break;
			}
		}
	}
	else
		m_session->stop_dht();
	
	m_session->set_max_uploads(getSettingsValue("torrent/maxuploads").toInt());
	m_session->set_max_connections(getSettingsValue("torrent/maxconnections").toInt());
	
	settings.file_pool_size = getSettingsValue("torrent/maxfiles").toInt();
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
	
	switch(getSettingsValue("torrent/enc_incoming").toInt())
	{
	case 0:
		ps.in_enc_policy = libtorrent::pe_settings::disabled; break;
	case 1:
		ps.in_enc_policy = libtorrent::pe_settings::enabled; break;
	case 2:
		ps.in_enc_policy = libtorrent::pe_settings::forced; break;
	}
	switch(getSettingsValue("torrent/enc_outgoing").toInt())
	{
	case 0:
		ps.out_enc_policy = libtorrent::pe_settings::disabled; break;
	case 1:
		ps.out_enc_policy = libtorrent::pe_settings::enabled; break;
	case 2:
		ps.out_enc_policy = libtorrent::pe_settings::forced; break;
	}
	
	switch(getSettingsValue("torrent/enc_level").toInt())
	{
	case 0:
		ps.allowed_enc_level = libtorrent::pe_settings::plaintext; break;
	case 1:
		ps.allowed_enc_level = libtorrent::pe_settings::rc4; break;
	case 2:
		ps.allowed_enc_level = libtorrent::pe_settings::both;
	}
	
	ps.prefer_rc4 = getSettingsValue("torrent/enc_rc4_prefer").toBool();
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
	
	m_seedLimitRatio = getSettingsValue("torrent/maxratio").toDouble();
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
			
			storageMode = (libtorrent::storage_mode_t) getSettingsValue("torrent/allocation").toInt();
			m_handle = m_session->add_torrent(boost::intrusive_ptr<libtorrent::torrent_info>(m_info), target.toStdString(), libtorrent::entry(), storageMode, !isActive());
			
			int limit;
			
			limit = getSettingsValue("torrent/maxuploads_loc").toInt();
			m_handle.set_max_uploads(limit ? limit : limit-1);
			
			limit = getSettingsValue("torrent/maxconnections_loc").toInt();
			m_handle.set_max_connections(limit ? limit : limit-1);
			
			m_bHasHashCheck = true;
			
			m_handle.set_ratio(1.2f);
			createDefaultPriorityList();
		
			m_worker->doWork();
			storeTorrent(source);
			
			if(!m_bAuto)
				RssFetcher::performManualCheck(name());
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
			qDebug() << "Downloading" << source << "to" << m_pFileDownloadTemp->fileName();
			dir.cdUp();
			
			QUrl url(source);
			m_pFileDownload->setHost(url.host(), url.port(80));
			connect(m_pFileDownload, SIGNAL(done(bool)), this, SLOT(torrentFileDone(bool)), Qt::QueuedConnection);
			
			int id;
			if(url.hasQuery())
				id = m_pFileDownload->get(url.path() + '?' + url.encodedQuery(), m_pFileDownloadTemp);
			else
				id = m_pFileDownload->get(url.path(), m_pFileDownloadTemp);
			
			qDebug() << "Download has begun" << id;
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
	qDebug() << "TorrentDownload::torrentFileDone";
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
			m_strTarget = target;
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
		{
			m_handle.resume();
			m_handle.force_reannounce();
		}
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
		return qMax<qint64>(0, /*m_status.total_wanted_done*/ m_totalWantedDone);
	else
		return 0;
}

qulonglong TorrentDownload::total() const
{
	if(m_handle.is_valid())
		return /*m_status.total_wanted*/ m_totalWanted;
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
	QVector<char> buf2 = QVector<char>::fromStdVector(buffer);
	return QByteArray(buf2.data(), buf2.size());
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
			m_seedLimitRatio = getSettingsValue("torrent/maxratio").toDouble();
		
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
		
		storageMode = (libtorrent::storage_mode_t) getSettingsValue("torrent/allocation").toInt();
		
		m_handle = m_session->add_torrent(boost::intrusive_ptr<libtorrent::torrent_info>( m_info ), str.toStdString(), torrent_resume, storageMode, true);
		
		m_handle.set_ratio(1.2f);
		m_handle.set_max_uploads(getSettingsValue("torrent/maxuploads").toInt());
		m_handle.set_max_connections(getSettingsValue("torrent/maxconnections").toInt());
		
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
		return tr("Downloading the .torrent file");
	
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
			state = tr("Connecting to the tracker");
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
	QMutexLocker l(&m_mutex);
	m_objects << d;
}

void TorrentWorker::removeObject(TorrentDownload* d)
{
	QMutexLocker l(&m_mutex);
	m_objects.removeAll(d);
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
	QMutexLocker l(&m_mutex);
	
	// libtorrent bug workaround
	foreach(TorrentDownload* d, m_objects)
	{
		if(!d->m_handle.is_valid())
			continue;
		
		std::vector<int> prio = d->m_handle.piece_priorities();
		qint64 done = 0, wanted = 0;
		
		d->m_status = d->m_handle.status();
		
		const std::vector<bool>* prog = d->m_status.pieces;
		int len = d->m_info->piece_length();
		
		for(size_t i=0;i<prog->size();i++)
		{
			if(prio[i])
			{
				if(i+1 == prog->size())
					len = d->m_info->piece_size(i);
				
				wanted += len;
				if(prog->at(i))
					done += len;
			}
		}
		
		d->m_totalWanted = wanted;
		d->m_totalWantedDone = done;
	}
	
	foreach(TorrentDownload* d, m_objects)
	{
		if(!d->m_handle.is_valid())
			continue;
		
		//d->m_status = d->m_handle.status();
		
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
					d->enterLogMessage(tr("The torrent has been downloaded"));
					d->m_handle.set_ratio(0);
				}
				//else if(d->m_status.total_wanted == d->m_status.total_wanted_done)
				else if(d->m_totalWanted == d->m_totalWantedDone)
				{
					d->enterLogMessage(tr("Requested parts of the torrent have been downloaded"));
					d->setMode(Transfer::Upload);
					d->m_handle.set_ratio(0);
				}
			}
			if(d->mode() == Transfer::Upload)
			{
				//if(d->m_status.total_wanted_done < d->m_status.total_wanted)
				if(d->m_totalWantedDone < d->m_totalWanted)
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

#define IS_ALERT(type) libtorrent::type* alert = dynamic_cast<libtorrent::type*>(aaa)
#define IS_ALERT_S(type) dynamic_cast<libtorrent::type*>(aaa) != 0
	
	while(true)
	{
		libtorrent::alert* aaa;
		std::auto_ptr<libtorrent::alert> a = TorrentDownload::m_session->pop_alert();
		
		if((aaa = a.get()) == 0)
			break;
		
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
				d->enterLogMessage(tr("The fast-resume data have been rejected: %1").arg(errmsg));
			}
		}
		else
		{
			Logger::global()->enterLogMessage("BitTorrent", aaa->msg().c_str());
			
			/*if(IS_ALERT(peer_error_alert))
			{
				// TODO:
				std::string ip = alert->ip.address().to_string();
				qDebug() << "\tFor IP address" << ip.c_str() << alert->ip.port();
			}*/
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
