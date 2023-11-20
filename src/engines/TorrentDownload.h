/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 3 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#ifndef TORRENTDOWNLOAD_H
#define TORRENTDOWNLOAD_H

#include "config.h"

#ifndef WITH_BITTORRENT
#error This file is not supposed to be included!
#endif

#include <QMutex>
#include <QRegularExpression>
#include <QTemporaryFile>
#include <QTimer>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_status.hpp>
#include <vector>

#include "Proxy.h"
#include "Transfer.h"
#include "WidgetHostChild.h"
#ifdef WITH_WEBINTERFACE
#include "remote/TransferHttpService.h"
#endif

class TorrentWorker;
class TorrentDetails;
class RssFetcher;
class QLabel;
class QNetworkAccessManager;
class QNetworkReply;

#ifndef WITH_WEBINTERFACE
class TorrentDownload : public Transfer
#else
class TorrentDownload : public Transfer,
                        public TransferHttpService
#endif

{
  Q_OBJECT
 public:
  TorrentDownload(bool bAuto = false);
  virtual ~TorrentDownload();

  static Transfer* createInstance() { return new TorrentDownload; }
  static WidgetHostChild* createSettingsWidget(QWidget* w, QIcon& i);
  static int acceptable(QString url, bool);

  static void globalInit();
  static void applySettings();
  static void globalExit();

  static QByteArray bencode_simple(libtorrent::entry& e);
  static QString bencode(libtorrent::entry& e);
  static libtorrent::bdecode_node bdecode_simple(QByteArray d);
  static libtorrent::bdecode_node bdecode(QString d);

  static libtorrent::proxy_settings proxyToLibtorrent(Proxy p);

  virtual void init(QString source, QString target);
  virtual void setObject(QString source);

  virtual void changeActive(bool nowActive);
  virtual void setSpeedLimits(int down, int up);

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
  virtual QString remoteURI() const;

  virtual QString dataPath(bool bDirect = true) const;

  qint64 totalDownload() const { return m_status.all_time_download; }
  qint64 totalUpload() const { return m_status.all_time_upload; }

  void addUrlSeed(QString str);

#ifdef WITH_WEBINTERFACE
  // TransferHttpService
  virtual void process(QString method, QMap<QString, QString> args,
                       WriteBack* wb);
  virtual const char* detailsScript() const;
  virtual QVariantMap properties() const;
#endif
 public slots:
  void downloadTorrent(QString source);

 private:
  void createDefaultPriorityList();
  bool storeTorrent(QString orig);
  bool storeTorrent();
  QString storedTorrentName() const;
  static void fillSettings(libtorrent::settings_pack& settings);
 private slots:
  void torrentFileDone(QNetworkReply* reply);
  void torrentFileReadyRead();
  void forceReannounce();
  void forceRecheck();
#ifdef WITH_WEBINTERFACE
 private:
  static QVariant setFilePriorities(QList<QVariant>& args);
#endif
 protected:
  libtorrent::torrent_handle m_handle;
  std::shared_ptr<const libtorrent::torrent_info> m_info;
  libtorrent::torrent_status m_status;

  QString m_strError, m_strTarget;
  // qint64 m_nPrevDownload, m_nPrevUpload;
  std::vector<libtorrent::download_priority_t> m_vecPriorities;
  bool m_bHasHashCheck, m_bAuto, m_bSuperSeeding;
  QList<QString> m_urlSeeds;

  QNetworkAccessManager* m_pFileDownload;
  QNetworkReply* m_pReply;
  QTemporaryFile* m_pFileDownloadTemp;

  // seeding limits
  double m_seedLimitRatio;
  int m_seedLimitUpload;

  static libtorrent::session* m_session;
  static TorrentWorker* m_worker;
  static bool m_bDHT;
  static QList<QRegularExpression> m_listBTLinks;
  static QLabel* m_labelDHTStats;
  static QMutex m_mutexAlerts;

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

class TorrentWorker : public QObject {
  Q_OBJECT
 public:
  TorrentWorker();
  void addObject(TorrentDownload* d);
  void removeObject(TorrentDownload* d);
  // To reduce QTimer usage
  void setDetailsObject(TorrentDetails* d);
  TorrentDownload* getByHandle(libtorrent::torrent_handle handle) const;
  void processAlert(libtorrent::alert* aaa);
 public slots:
  void doWork();

 private:
  QTimer m_timer, m_timerStats;
  QMutex m_mutex;
  QList<TorrentDownload*> m_objects;
};

#endif
