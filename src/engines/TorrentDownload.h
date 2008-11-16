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
*/

#ifndef TORRENTDOWNLOAD_H
#define TORRENTDOWNLOAD_H

#include "config.h"

#ifndef WITH_BITTORRENT
#	error This file is not supposed to be included!
#endif

#include "Transfer.h"
#include "WidgetHostChild.h"
#include <QTimer>
#include <QMutex>
#include <QHttp>
#include <QTemporaryFile>
#include <QRegExp>
#include <vector>
#include <libtorrent/session.hpp>
#include <libtorrent/torrent_handle.hpp>
#include "Proxy.h"

class TorrentWorker;
class TorrentDetails;
class RssFetcher;
class QLabel;

class TorrentDownload : public Transfer
{
Q_OBJECT
public:
	TorrentDownload(bool bAuto = false);
	virtual ~TorrentDownload();
	
	static Transfer* createInstance() { return new TorrentDownload; }
	static WidgetHostChild* createSettingsWidget(QWidget* w,QIcon& i);
	static int acceptable(QString url, bool);
	
	static void globalInit();
	static void applySettings();
	static void globalExit();
	
	static QByteArray bencode_simple(libtorrent::entry& e);
	static QString bencode(libtorrent::entry& e);
	static libtorrent::entry bdecode_simple(QByteArray d);
	static libtorrent::entry bdecode(QString d);
	
	static libtorrent::proxy_settings proxyToLibtorrent(Proxy p);
	
	virtual void init(QString source, QString target);
	virtual void setObject(QString source);
	
	virtual void changeActive(bool nowActive);
	virtual void setSpeedLimits(int down, int up);
	
	virtual QString dataPath(bool bDirect) const;
	
	virtual QString object() const;
	virtual QString myClass() const { return "TorrentDownload"; }
	virtual QString name() const;
	virtual QString message() const;
	virtual void speeds(int& down, int& up) const;
	virtual qulonglong total() const;
	virtual qulonglong done() const;
	
	virtual void load(const QDomNode& map);
	virtual void save(QDomDocument& doc, QDomNode& map) const;
	virtual void fillContextMenu(QMenu& menu);
	virtual QObject* createDetailsWidget(QWidget* widget);
	virtual WidgetHostChild* createOptionsWidget(QWidget* w);
	
	qint64 totalDownload() const { return m_nPrevDownload + m_status.total_payload_download; }
	qint64 totalUpload() const { return m_nPrevUpload + m_status.total_payload_upload; }
public slots:
	void downloadTorrent(QString source);
private:
	void createDefaultPriorityList();
	bool storeTorrent(QString orig);
	QString storedTorrentName() const;
private slots:
	void torrentFileDone(bool error);
	void forceReannounce();
	void forceRecheck();
protected:
	libtorrent::torrent_handle m_handle;
	boost::intrusive_ptr<libtorrent::torrent_info> m_info;
	libtorrent::torrent_status m_status;
	
	QString m_strError, m_strTarget;
	qint64 m_nPrevDownload, m_nPrevUpload;
	std::vector<int> m_vecPriorities;
	bool m_bHasHashCheck, m_bAuto;
	
	QHttp* m_pFileDownload;
	QTemporaryFile* m_pFileDownloadTemp;
	
	// seeding limits
	double m_seedLimitRatio;
	int m_seedLimitUpload;
	
	static libtorrent::session* m_session;
	static TorrentWorker* m_worker;
	static bool m_bDHT;
	static QList<QRegExp> m_listBTLinks;
	static QLabel* m_labelDHTStats;
	
	friend class TorrentWorker;
	friend class TorrentDetails;
	friend class TorrentPiecesModel;
	friend class TorrentPeersModel;
	friend class TorrentFilesModel;
	friend class TorrentProgressDelegate;
	friend class TorrentOptsWidget;
	friend class TorrentSettings;
	friend class SettingsRssForm;
};

class TorrentWorker : public QObject
{
Q_OBJECT
public:
	TorrentWorker();
	void addObject(TorrentDownload* d);
	void removeObject(TorrentDownload* d);
	// To reduce QTimer usage
	void setDetailsObject(TorrentDetails* d);
	TorrentDownload* getByHandle(libtorrent::torrent_handle handle) const;
public slots:
	void doWork();
private:
	QTimer m_timer;
	QMutex m_mutex;
	QList<TorrentDownload*> m_objects;
};

#endif
