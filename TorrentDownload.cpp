#include "fatrat.h"
#include "TorrentDownload.h"
#include "TorrentSettings.h"
#include "RuntimeException.h"
#include "GeneralDownload.h"
#include <QIcon>
#include <QMenu>
#include <QTemporaryFile>
#include <fstream>
#include <stdexcept>
#include <libtorrent/bencode.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/extensions/ut_pex.hpp>

extern QSettings* g_settings;
libtorrent::session* TorrentDownload::m_session = 0;
TorrentWorker* TorrentDownload::m_worker = 0;
bool TorrentDownload::m_bDHT = false;

TorrentDownload::TorrentDownload()
	: m_nPrevDownload(0), m_nPrevUpload(0), m_pFileDownload(0)
{
	m_worker->addObject(this);
}

TorrentDownload::~TorrentDownload()
{
	m_worker->removeObject(this);
	if(m_handle.is_valid())
		m_session->remove_torrent(m_handle);
}

int TorrentDownload::acceptable(QString uri)
{
	const bool istorrent = uri.endsWith(".torrent", Qt::CaseInsensitive);
        if(uri[0] == '/' || uri.startsWith("http://") || uri.startsWith("ftp://"))
                return (istorrent) ? 3 : 2;
        return 0;
}

void TorrentDownload::globalInit()
{
	m_session = new libtorrent::session(libtorrent::fingerprint("FR", 0, 1, 0, 0));
	m_session->set_severity_level(libtorrent::alert::warning);
	
	applySettings();
	
	if(g_settings->value("torrent/pex", getSettingsDefault("torrent/pex")).toBool())
		m_session->add_extension(&libtorrent::create_ut_pex_plugin);
	
	m_worker = new TorrentWorker;
}

void TorrentDownload::applySettings()
{
	g_settings->beginGroup("torrent");
	
	int lstart,lend;
	
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
			m_session->add_dht_node(std::pair<std::string, int>("router.bittorrent.com", 6881));
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
	
	m_session->set_max_uploads(g_settings->value("maxuploads", getSettingsDefault("torrent/maxuploads")).toInt());
	m_session->set_max_connections(g_settings->value("maxconnections", getSettingsDefault("torrent/maxconnections")).toInt());
	
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

void TorrentDownload::init(QString source, QString target)
{
	m_strTarget = target;
	
	try
	{
		if(source.startsWith('/'))
		{
			std::string file = source.toStdString();
			std::ifstream in(file.c_str(), std::ios_base::binary);
			in.unsetf(std::ios_base::skipws);
			
			m_info = libtorrent::bdecode(std::istream_iterator<char>(in), std::istream_iterator<char>());
			
			qDebug() << "Downloading to directory" << target;
			m_handle = m_session->add_torrent(m_info, target.toStdString(), false);
		
			if(!isActive())
				m_handle.pause();
			m_worker->doWork();
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
			qDebug() << "Downloading torrent to" <<temp->fileName();
			dir.cdUp();
			
			connect(download, SIGNAL(stateChanged(Transfer::State,Transfer::State)), this, SLOT(fileStateChanged(Transfer::State,Transfer::State)));
			download->init(source, dir.path());
			download->setTargetName(name);
			download->setState(Active);
		}
	}
	catch(const std::exception& e)
	{
		throw RuntimeException(e.what());
	}
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
		if(!m_handle.move_storage(newplace))
			throw RuntimeException(tr("Cannot change storage!"));
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
	else if(m_pFileDownload != 0)
	{
		qDebug() << "changeActive() and the handle is not valid";
		setState(Failed);
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
	}
}

qulonglong TorrentDownload::done() const
{
	if(m_handle.is_valid())
	{
		qulonglong val = m_handle.status().num_pieces * m_info.piece_length();
		
		return (val < qulonglong(m_info.total_size())) ? val : m_info.total_size();
	}
	else if(m_pFileDownload != 0)
		return m_pFileDownload->done();
	else
		return 0;
}

qulonglong TorrentDownload::total() const
{
	if(m_handle.is_valid())
		return m_info.total_size();
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
		QString target;
		
		m_strTarget = target = getXMLProperty(map, "target");
		
		m_info = bdecode(getXMLProperty(map, "torrent_data"));
		torrent_resume = bdecode(getXMLProperty(map, "torrent_resume"));
		
		qDebug() << "Downloading to directory" << target;
		m_handle = m_session->add_torrent(m_info, target.toStdString(), torrent_resume);
			
		if(!isActive())
			m_handle.pause();
		
		m_nPrevDownload = getXMLProperty(map, "downloaded").toLongLong();
		m_nPrevUpload = getXMLProperty(map, "uploaded").toLongLong();
		
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
	
	setXMLProperty(doc, map, "torrent_data", bencode(m_info.create_torrent()));
	
	if(m_handle.is_valid())
		setXMLProperty(doc, map, "torrent_resume", bencode(m_handle.write_resume_data()));
	
	setXMLProperty(doc, map, "target", object());
	setXMLProperty(doc, map, "downloaded", QString::number( totalDownload() ));
	setXMLProperty(doc, map, "uploaded", QString::number( totalUpload() ));
}

QString TorrentDownload::message() const
{
	QString state;
	
	if(this->state() == Failed)
		return m_strError;
	
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
			break;
		case libtorrent::torrent_status::seeding:
		case libtorrent::torrent_status::downloading:
			state = QString("Seeds: %1 (%2) | Peers: %3 (%4)");
			
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
		
		if(d->isActive())
		{
			if(d->mode() == Transfer::Download)
			{
				if(d->m_status.state == libtorrent::torrent_status::finished)
				{
					d->setState(Transfer::Completed);
					d->setMode(Transfer::Download);
				}
				else if(d->m_status.state == libtorrent::torrent_status::seeding)
					d->setMode(Transfer::Upload);
			}
			else if(d->state() != Transfer::ForcedActive)
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
		else
		{
			// TODO: what to do with this?
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

void TorrentWorker::setDetailsObject(TorrentDetails* d)
{
	connect(&m_timer, SIGNAL(timeout()), d, SLOT(refresh()));
}

/////////////////////////////////////////////

TorrentDetails::TorrentDetails(QWidget* me, TorrentDownload* obj)
	: m_download(obj), m_bFilled(false)
{
	setupUi(me);
	refresh();
	TorrentDownload::m_worker->setDetailsObject(this);
}

TorrentDetails::~TorrentDetails()
{
}

void TorrentDetails::fill()
{
	if(m_download->m_handle.is_valid())
	{
		m_bFilled = true;
		
		boost::optional<boost::posix_time::ptime> time = m_download->m_info.creation_date();
		if(time)
		{
			std::string created = boost::posix_time::to_simple_string(time.get());
			labelCreationDate->setText(created.c_str());
		}
		labelPieceLength->setText( QString("%1 kB").arg(m_download->m_info.piece_length()/1024.f) );
		lineComment->setText(m_download->m_info.comment().c_str());
		lineCreator->setText(m_download->m_info.creator().c_str());
		labelPrivate->setText( m_download->m_info.priv() ? tr("yes") : tr("no"));
	}
}

void TorrentDetails::refresh()
{
	if(m_download->m_handle.is_valid())
	{
		if(!m_bFilled)
			fill();
		
		boost::posix_time::time_duration& next = m_download->m_status.next_announce;
		boost::posix_time::time_duration& intv = m_download->m_status.announce_interval;
		
		labelAvailability->setText(QString::number(m_download->m_status.distributed_copies));
		labelTracker->setText(tr("%1 (refresh in %2:%3:%4, every %5:%6:%7)")
				.arg(m_download->m_status.current_tracker.c_str())
				.arg(next.hours()).arg(next.minutes(),2,10,QChar('0')).arg(next.seconds(),2,10,QChar('0'))
				.arg(intv.hours()).arg(intv.minutes(),2,10,QChar('0')).arg(intv.seconds(),2,10,QChar('0')));
		
		const std::vector<bool>* pieces = m_download->m_status.pieces;
		
		if(pieces != 0 && m_vecPieces != (*pieces))
		{
			widgetCompletition->generate(*pieces);
			m_vecPieces = *pieces;
		}
	}
}

