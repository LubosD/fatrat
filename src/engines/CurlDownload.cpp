/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2011 Lubos Dolezel <lubos a dolezel.info>

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

#include "CurlDownload.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <QColor>
#include <QMenu>
#include <QMessageBox>
#include <QRandomGenerator>
#include <QtDebug>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>

#include "Auth.h"
#include "CurlPoller.h"
#include "GeneralDownloadForms.h"
#include "HttpDetails.h"
#include "HttpFtpSettings.h"
#include "RuntimeException.h"
#include "Settings.h"
#include "config.h"
#include "engines/CurlPollingMaster.h"
#include "tools/HashDlg.h"
#include "util/ExtendedAttributes.h"

#ifdef WITH_BITTORRENT
#include "TorrentDownload.h"
#endif
#ifndef POSIX_LINUX
#define O_LARGEFILE 0
#endif

static const QColor g_colors[] = {
    Qt::red,      Qt::green,    Qt::blue,        Qt::cyan,
    Qt::magenta,  Qt::yellow,   Qt::darkRed,     Qt::darkGreen,
    Qt::darkBlue, Qt::darkCyan, Qt::darkMagenta, Qt::darkYellow};

CurlDownload::CurlDownload()
    : m_nTotal(0),
      m_nStart(0),
      m_bAutoName(false),
      m_segmentsLock(QReadWriteLock::Recursive),
      m_master(0),
      m_nameChanger(0) {
  m_errorBuffer[0] = 0;
  connect(&m_timer, SIGNAL(timeout()), this, SLOT(updateSegmentProgress()));
}

CurlDownload::~CurlDownload() {
  if (isActive()) changeActive(false);
}

void CurlDownload::init(QString uri, QString dest) {
  UrlClient::UrlObject obj;

  int hash = uri.lastIndexOf("#__filename=");
  if (hash != -1) {
    QString name = QUrl::fromPercentEncoding(uri.mid(hash + 12).toUtf8());
    m_strFile = name;
    uri.resize(hash);
  }
  hash = uri.indexOf('#');
  if (hash != -1) uri.resize(hash);

  obj.url = QUrl::fromPercentEncoding(uri.toUtf8());

  if (obj.url.userInfo().isEmpty()) {
    QList<Auth> auths = Auth::loadAuths();
    foreach (Auth a, auths) {
      if (!QRegularExpression(a.strRegExp).match(uri).hasMatch()) continue;

      obj.url.setUserName(a.strUser);
      obj.url.setPassword(a.strPassword);

      enterLogMessage(tr("Loaded stored authentication data, matched regexp %1")
                          .arg(a.strRegExp));

      break;
    }
  }

  obj.proxy =
      QUuid::fromString(getSettingsValue("httpftp/defaultproxy").toString());
  obj.ftpMode = UrlClient::FtpPassive;

  m_dir.setPath(dest);
  m_dir.mkpath(".");

  QString scheme = obj.url.scheme();
  if (scheme != "http" && scheme != "ftp" && scheme != "ftps" &&
      scheme != "sftp" && scheme != "https")
    throw RuntimeException(tr("Unsupported protocol: \"%1\"").arg(scheme));

  m_urls.clear();
  m_urls << obj;
  m_listActiveSegments << 0;

  if (m_strFile.isEmpty()) generateName();
}

void CurlDownload::generateName() {
  QString name;
  if (!m_urls.isEmpty()) name = QFileInfo(m_urls[0].url.path()).fileName();
  name = QUrl::fromPercentEncoding(name.toUtf8());

  int pos = name.indexOf('?');
  if (pos != -1) name = name.left(pos);

  m_strFile =
      (!name.isEmpty() && name != "/" && name != ".") ? name : "default.html";
  assert(!m_strFile.isEmpty());
  m_bAutoName = true;

  qDebug() << "Generated file name:" << m_strFile;
}

int CurlDownload::acceptable(QString uri, bool) {
  QUrl url = uri;
  QString scheme = url.scheme();

  if (scheme != "http" && scheme != "ftp" && scheme != "ftps" &&
      scheme != "https" && scheme != "sftp")
    return 0;
  else
    return 2;
}

void CurlDownload::globalInit() {
  new CurlPoller;

  CurlPoller::setTransferTimeout(getSettingsValue("httpftp/timeout").toInt());

  SettingsItem si;
  si.icon = DelayedIcon(":/fatrat/httpftp.png");
  si.title = tr("HTTP/FTP");
  si.lpfnCreate = HttpFtpSettings::create;

  addSettingsPage(si);
}

void CurlDownload::globalExit() { delete CurlPoller::instance(); }

void CurlDownload::setObject(QString target) {
  QDir dirnew = target;
  if (dirnew != m_dir) {
    if (!QFile::rename(filePath(), target + "/" + m_strFile))
      throw RuntimeException(tr("Cannot move the file."));

    m_dir = dirnew;
  }
}

void CurlDownload::init2(QString uri, QString dest) {
  QDir dir(dest);
  m_strFile = dir.dirName();
  dir.cdUp();
  init(uri, dir.path());
}

QString CurlDownload::object() const { return m_dir.path(); }

QString CurlDownload::name() const {
  return !m_strFile.isEmpty() ? m_strFile : "default.html";
}

int anti_crash_fun() { return 0; }

void CurlDownload::changeActive(bool bActive) {
  qDebug() << this << "changeActive" << bActive;
  if (bActive) {
    autoCreateSegment();

    const qlonglong d = done();
    m_strMessage.clear();

    if (m_urls.isEmpty()) {
      enterLogMessage(m_strMessage = tr("No URLs have been given"));
      setState(Failed);
      return;
    }

    m_nameChanger = 0;

    QWriteLocker l(&m_segmentsLock);

    simplifySegments(m_segments);

    if (m_segments.size() == 1 && m_nTotal == d && d) {
      setState(Completed);
      return;
    }

    m_master = new CurlPollingMaster;
    CurlPoller::instance()->addTransfer(m_master);
    m_master->setMaxDown(m_nDownLimitInt);

    qDebug() << "The limit is" << m_nDownLimitInt;

    fixActiveSegmentsList();

    if (m_nTotal) {
      for (int i = 0; i < m_listActiveSegments.size(); i++)
        startSegment(m_listActiveSegments[i]);
    } else
      startSegment(m_listActiveSegments[0]);

    // 8) update the segment progress every 500 miliseconds
    m_timer.start(500);
  } else if (m_master != 0) {
    updateSegmentProgress();

    m_segmentsLock.lockForWrite();
    for (int i = 0; i < m_segments.size(); i++) {
      if (!m_segments[i].client) continue;
      m_master->removeTransfer(m_segments[i].client);
      m_segments[i].client->stop();
      // delete m_segments[i].client;
      m_segments[i].client = 0;
      m_segments[i].color = Qt::black;
    }
    qDebug() << "Before final simplify segments:" << m_segments;
    simplifySegments(m_segments);
    qDebug() << "After final simplify segments:" << m_segments;
    m_segmentsLock.unlock();
    m_nameChanger = 0;
    m_timer.stop();

    CurlPoller::instance()->removeTransfer(m_master);
    // delete m_master;
    m_master = 0;
  }
}

void CurlDownload::startSegment(Segment& seg, qlonglong bytes) {
  qDebug() << "CurlDownload::startSegment(): seg offset:" << seg.offset
           << "; bytes:" << bytes;

  if (!bytes) abort();  // this is a serious bug

  std::string spath = filePath().toStdString();
  int file = open(spath.c_str(), O_CREAT | O_RDWR | O_LARGEFILE, 0666);
  if (file < 0) {
    enterLogMessage(m_strMessage = strerror(errno));
    setState(Failed);
    return;
  }
  ExtendedAttributes::setAttribute(filePath(),
                                   ExtendedAttributes::ATTR_ORIGIN_URL,
                                   m_urls[0].url.toString().toUtf8());

  seg.client = new UrlClient;
  seg.client->setRange(seg.offset, (bytes > 0) ? seg.offset + bytes : -1);
  seg.client->setSourceObject(m_urls[seg.urlIndex]);
  seg.client->setTargetObject(file);

  connect(seg.client, SIGNAL(renameTo(QString)), this,
          SLOT(clientRenameTo(QString)));
  connect(seg.client, SIGNAL(logMessage(QString)), this,
          SLOT(clientLogMessage(QString)));
  connect(seg.client, SIGNAL(done(QString)), this, SLOT(clientDone(QString)));
  connect(seg.client, SIGNAL(failure(QString)), this,
          SLOT(clientFailure(QString)));
  connect(seg.client, SIGNAL(totalSizeKnown(qlonglong)), this,
          SLOT(clientTotalSizeKnown(qlonglong)));
  connect(seg.client, SIGNAL(rangesUnsupported()), this,
          SLOT(clientRangesUnsupported()));

  seg.client->setPollingMaster(m_master);
  seg.client->start();
  m_master->addTransfer(static_cast<CurlUser*>(seg.client));
}

bool CurlDownload::Segment::operator<(const Segment& s2) const {
  return this->offset < s2.offset;
}

bool CurlDownload::FreeSegment::operator<(const FreeSegment& s2) const {
  return this->bytes < s2.bytes;
}

bool CurlDownload::FreeSegment::compareByOffset(const FreeSegment& s1,
                                                const FreeSegment& s2) {
  return s1.offset < s2.offset;
}

void CurlDownload::setTargetName(QString newFileName) {
  if (m_strFile != newFileName) {
    m_dir.rename(m_strFile, newFileName);
    m_strFile = newFileName;
  }
}

void CurlDownload::speeds(int& down, int& up) const {
  down = up = 0;
  if (m_master != 0) m_master->speeds(down, up);
}

qulonglong CurlDownload::total() const { return m_nTotal; }

qulonglong CurlDownload::done() const {
  m_segmentsLock.lockForRead();
  qlonglong total = 0;

  for (int i = 0; i < m_segments.size(); i++) total += m_segments[i].bytes;

  m_segmentsLock.unlock();
  return total;
}

void CurlDownload::load(const QDomNode& map) {
  m_dir.setPath(getXMLProperty(map, "dir"));
  m_nTotal = getXMLProperty(map, "knowntotal").toULongLong();
  m_strFile = getXMLProperty(map, "filename");
  m_bAutoName = getXMLProperty(map, "autoname").toInt() != 0;

  QStringList activeSegments = getXMLProperty(map, "activesegments").split(',');
  m_listActiveSegments.clear();
  foreach (QString seg, activeSegments) m_listActiveSegments << seg.toInt();

  QDomElement url = map.firstChildElement("url");
  while (!url.isNull()) {
    UrlClient::UrlObject obj;

    obj.url = getXMLProperty(url, "address");
    obj.strReferrer = getXMLProperty(url, "referrer");
    obj.proxy = QUuid::fromString(getXMLProperty(url, "proxy"));
    obj.ftpMode = (UrlClient::FtpMode)getXMLProperty(url, "ftpmode").toInt();
    obj.strBindAddress = getXMLProperty(url, "bindip");
    obj.effective = getXMLProperty(url, "effective");

    url = url.nextSiblingElement("url");

    m_urls << obj;
  }

  QDomElement segment, segments = map.firstChildElement("segments");

  m_segmentsLock.lockForWrite();
  if (!segments.isNull()) segment = segments.firstChildElement("segment");
  while (!segment.isNull()) {
    Segment data;

    data.offset = getXMLProperty(segment, "offset").toLongLong();
    data.bytes = getXMLProperty(segment, "bytes").toLongLong();
    // data.urlIndex = getXMLProperty(segment, "urlindex").toInt();
    data.urlIndex = -1;
    data.client = 0;

    segment = segment.nextSiblingElement("segment");
    m_segments << data;
  }

  if (m_strFile.isEmpty()) generateName();

  autoCreateSegment();
  m_segmentsLock.unlock();

  Transfer::load(map);
}

void CurlDownload::save(QDomDocument& doc, QDomNode& map) const {
  Transfer::save(doc, map);

  setXMLProperty(doc, map, "dir", m_dir.path());
  setXMLProperty(doc, map, "knowntotal", QString::number(m_nTotal));
  setXMLProperty(doc, map, "filename", m_strFile);
  setXMLProperty(doc, map, "autoname", QString::number(m_bAutoName));

  for (int i = 0; i < m_urls.size(); i++) {
    QDomElement sub = doc.createElement("url");
    // sub.setAttribute("id", QString::number(i));
    const UrlClient::UrlObject& url = m_urls[i];

    setXMLProperty(doc, sub, "address", QString(url.url.toString()));
    setXMLProperty(doc, sub, "effective", QString(url.effective.toEncoded()));
    setXMLProperty(doc, sub, "referrer", url.strReferrer);
    setXMLProperty(doc, sub, "proxy", url.proxy.toString());
    setXMLProperty(doc, sub, "ftpmode", QString::number((int)url.ftpMode));
    setXMLProperty(doc, sub, "bindip", url.strBindAddress);

    map.appendChild(sub);
  }

  QDomElement subSegments = doc.createElement("segments");

  m_segmentsLock.lockForRead();
  for (int i = 0; i < m_segments.size(); i++) {
    QDomElement sub = doc.createElement("segment");
    const Segment& s = m_segments[i];

    setXMLProperty(doc, sub, "offset", QString::number(s.offset));
    setXMLProperty(doc, sub, "bytes", QString::number(s.bytes));
    // setXMLProperty(doc, sub, "urlindex", QString::number(s.urlIndex));

    subSegments.appendChild(sub);
  }
  m_segmentsLock.unlock();
  map.appendChild(subSegments);

  QString activeSegments;
  foreach (int index, m_listActiveSegments) {
    if (!activeSegments.isEmpty()) activeSegments += ',';
    activeSegments += QString::number(index);
  }
  setXMLProperty(doc, map, "activesegments", activeSegments);
}

void CurlDownload::autoCreateSegment() {
  QFileInfo fi(filePath());

  if (!fi.exists()) {
    m_segments.clear();
    return;
  }

  if (m_segments.isEmpty()) {
    Segment s;

    s.offset = s.bytes = 0;
    s.urlIndex = 0;
    s.client = 0;

    s.bytes = fi.size();
    if (s.bytes) m_segments << s;
  } else {
    // check for segments beyond the EOF (truncated file)
    std::sort(m_segments.begin(), m_segments.end());
    for (int i = 0; i < m_segments.size(); i++) {
      Segment& s = m_segments[i];
      if (s.offset >= fi.size())
        m_segments.removeAt(i--);
      else if (s.offset + s.bytes > fi.size())
        s.bytes = fi.size() - s.offset;
    }
  }
}

void CurlDownload::updateSegmentProgress() {
  m_segmentsLock.lockForWrite();
  for (int i = 0; i < m_segments.size(); i++) {
    if (m_segments[i].client != 0)
      m_segments[i].bytes = m_segments[i].client->progress();
  }
  simplifySegments(m_segments);
  m_segmentsLock.unlock();
}

void CurlDownload::fillContextMenu(QMenu& menu) {
  QAction* a;

  // a = menu.addAction(tr("Switch mirror"));
  // connect(a, SIGNAL(triggered()), this, SLOT(switchMirror()));

  a = menu.addAction(tr("Compute hash..."));
  connect(a, SIGNAL(triggered()), this, SLOT(computeHash()));
}

/*
void CurlDownload::switchMirror()
{
        int prev,cur;
        prev = cur = m_nUrl;

        cur++;
        if(cur >= m_urls.size())
                cur = 0;

        if(cur == prev)
                enterLogMessage(tr("No mirror to switch to!"));
        else
        {
                enterLogMessage(tr("Switching mirror: %1 ->
%2").arg(m_urls[prev].url.toString()).arg(m_urls[cur].url.toString())); m_nUrl =
cur;

                if(isActive())
                {
                        changeActive(false);
                        changeActive(true);
                }
        }
}
*/

void CurlDownload::computeHash() {
  if (state() != Completed) {
    if (QMessageBox::warning(
            getMainWindow(), "FatRat",
            tr("You're about to compute hash from an incomplete download."),
            QMessageBox::Ok | QMessageBox::Cancel) != QMessageBox::Ok)
      return;
  }

  HashDlg dlg(getMainWindow(), filePath());
  dlg.exec();
}

QString CurlDownload::filePath() const { return m_dir.filePath(name()); }

void CurlDownload::setSpeedLimits(int down, int) {
  if (m_master != 0) m_master->setMaxDown(down);
}

QDialog* CurlDownload::createMultipleOptionsWidget(
    QWidget* parent, QList<Transfer*>& transfers) {
  HttpUrlOptsDlg* obj = new HttpUrlOptsDlg(parent, &transfers);
  obj->init();
  return obj;
}

WidgetHostChild* CurlDownload::createOptionsWidget(QWidget* w) {
  return new HttpOptsWidget(w, this);
}

QString CurlDownload::remoteURI() const {
  if (m_urls.isEmpty()) return QString();
  QUrl url = m_urls[0].url;
  url.setUserInfo(QString());
  return url.toString();
}

void CurlDownload::simplifySegments(QList<CurlDownload::Segment>& retval) {
  std::sort(retval.begin(), retval.end());

  for (int i = 1; i < retval.size(); i++) {
    qlonglong pos = retval[i - 1].offset + retval[i - 1].bytes;
    if (retval[i].offset <= pos && !retval[i - 1].client && !retval[i].client) {
      retval[i].bytes += retval[i].offset - retval[i - 1].offset;
      retval[i].offset = retval[i - 1].offset;
      retval.removeAt(--i);
    }
  }
}

void CurlDownload::fixActiveSegmentsList() {
  for (int i = 0; i < m_listActiveSegments.size(); i++) {
    if (m_listActiveSegments[i] < 0 ||
        m_listActiveSegments[i] >= m_urls.size()) {
      m_listActiveSegments.removeAt(i);
      i--;
    }
  }
  if (m_listActiveSegments.isEmpty()) m_listActiveSegments << 0;
}

void CurlDownload::clientRenameTo(QString name) {
  UrlClient* client = static_cast<UrlClient*>(sender());
  if (m_nameChanger && client != m_nameChanger) return;

  name = QUrl::fromPercentEncoding(name.toUtf8());

  int pos = name.indexOf('?');
  if (pos != -1) name = name.left(pos);

  m_nameChanger = client;
  if (m_bAutoName) setTargetName(name);
}

void CurlDownload::clientLogMessage(QString msg) {
  UrlClient* client = static_cast<UrlClient*>(sender());
  enterLogMessage(QString("0x%1 - %2").arg(long(client), 0, 16).arg(msg));
}

void CurlDownload::clientTotalSizeKnown(qlonglong bytes) {
  qDebug() << "CurlDownload::clientTotalSizeKnown()" << bytes
           << "segs:" << m_listActiveSegments.size();
  if (!m_nTotal && m_listActiveSegments.size() > 1) {
    qDebug() << "Starting aditional segments";
    m_nTotal = bytes;
    // there are active segments we need to initialize now
    for (int i = 1; i < m_listActiveSegments.size(); i++)
      startSegment(m_listActiveSegments[i]);
  } else
    m_nTotal = bytes;
}

void CurlDownload::clientRangesUnsupported() {
  // Several considerations:
  // 1) This error may be caused by a merely one invalid link in the mirror
  // list,
  //    the resume may be actually supported elsewhere
  // 2) Ranges are apparently supported if we already have multiple downloaded
  //    segments

  UrlClient* client = static_cast<UrlClient*>(sender());

  m_segmentsLock.lockForWrite();
  bool allfailed = true;
  int urlIndex = 0;

  // TODO: copy pasted from clientDone(), find a better way...
  for (int i = 0; i < m_segments.size(); i++) {
    if (m_segments[i].client == client) {
      m_segments[i].bytes = client->progress();
      m_segments[i].client = 0;
      urlIndex = m_segments[i].urlIndex;
      m_segments[i].urlIndex = -1;
      m_segments[i].color = Qt::black;
    }
  }

  simplifySegments(m_segments);

  for (int i = 0; i < m_segments.size(); i++) {
    if (m_segments[i].client != 0) {
      allfailed = false;
      break;
    }
  }

  m_segmentsLock.unlock();

  m_master->removeTransfer(client);
  client->stop();

  if (allfailed) {
    if (m_segments.size() == 1) {
      // restart the download from 0
      m_segments[0].bytes = 0;
      startSegment(urlIndex);
    } else {
      m_strMessage = tr("Unable to resume the download");
      setState(Failed);
    }
  }
}

void CurlDownload::clientFailure(QString err) {
  if (!isActive() || !m_master) return;

  qDebug() << "CurlDownload::clientFailure()" << err;
  m_strMessage = err;
  setState(Failed);
}

void CurlDownload::clientDone(QString error) {
  if (!isActive() || !m_master) return;

  UrlClient* client = static_cast<UrlClient*>(sender());
  int urlIndex = 0;

  updateSegmentProgress();

  m_segmentsLock.lockForWrite();

  qDebug() << "---------- CurlDownload::clientDone():" << error << client;

  for (int i = 0; i < m_segments.size(); i++) {
    if (m_segments[i].client == client) {
      m_segments[i].bytes = client->progress();
      m_segments[i].client = 0;
      urlIndex = m_segments[i].urlIndex;
      m_segments[i].urlIndex = -1;
      m_segments[i].color = Qt::black;
    }
  }

  simplifySegments(m_segments);

  m_segmentsLock.unlock();

  m_master->removeTransfer(client);
  client->stop();
  // client->deleteLater();

  qulonglong d = done();
  if ((d == total() && d) || (!total() && error.isNull())) {
    checkFileContents();
    setState(Completed);
  } else if (!error.isNull()) {
    m_segmentsLock.lockForRead();
    bool allfailed = true;

    for (int i = 0; i < m_segments.size(); i++) {
      if (m_segments[i].client != 0) {
        allfailed = false;
        break;
      }
    }

    m_segmentsLock.unlock();

    if (allfailed) {
      setState(Failed);
      m_strMessage = error;
    } else {
      // TODO: show error
      // TODO: Replace segment?
      m_listActiveSegments.removeOne(urlIndex);
    }
  } else {
    // The segment has been completed and the download is still incomplete
    // We need to find another free spot or split an allocated one
    // int down, up;
    // speeds(down, up);

    // Only if it has a meaning
    if (total() - done() * 2 >=
            (qlonglong)getSettingsValue("httpftp/minsegsize").toInt() ||
        m_listActiveSegments.size() == 1)
      startSegment(urlIndex);
    else
      m_listActiveSegments.removeOne(urlIndex);
  }
}

void CurlDownload::startSegment(int urlIndex) {
  QWriteLocker l(&m_segmentsLock);
  qDebug() << "----------- CurlDownload::startSegment():" << urlIndex;

  // 1) find free spots, prefer unallocated free spots
  QList<FreeSegment> freeSegs, freeSegsUnallocated;
  qlonglong lastEnd = 0;
  Segment seg;
  qlonglong bytes;

  updateSegmentProgress();

  seg.color = allocateSegmentColor();
  seg.bytes = 0;
  seg.urlIndex = urlIndex;

  if (!m_nTotal) {
    bytes = -1;
    seg.offset = (!m_segments.isEmpty()) ? m_segments[0].bytes : 0;
  }
  // No priority mode for downloads with a single thread
  else if (!getSettingsValue("httpftp/priority_mode", false).toBool() ||
           m_listActiveSegments.isEmpty()) {
    for (int i = 0; i < m_segments.size(); i++) {
      if (m_segments[i].offset > lastEnd) {
        FreeSegment fs(lastEnd, m_segments[i].offset - lastEnd);
        if (i == 0 || !m_segments[i - 1].client)
          freeSegsUnallocated << fs;
        else {
          fs.affectedClient = m_segments[i - 1].client;
          freeSegs << fs;
        }
      }
      lastEnd = m_segments[i].offset + m_segments[i].bytes;
    }
    if (lastEnd < m_nTotal) {
      FreeSegment fs(lastEnd, m_nTotal - lastEnd);
      if (!m_segments.isEmpty() && m_segments[m_segments.size() - 1].client) {
        fs.affectedClient = m_segments[m_segments.size() - 1].client;
        freeSegs << fs;
      } else
        freeSegsUnallocated << fs;
    }

    // 2) sort them
    std::sort(freeSegs.begin(), freeSegs.end());
    std::sort(freeSegsUnallocated.begin(), freeSegsUnallocated.end());

    if (!freeSegsUnallocated.isEmpty()) {
      // 3) use the smallest unallocated segment
      seg.offset = freeSegsUnallocated[0].offset;
      bytes = freeSegsUnallocated[0].bytes;
    } else if (!freeSegs.isEmpty()) {
      // 4) split the biggest free spot into halves
      FreeSegment& fs = freeSegs[freeSegs.size() - 1];
      // int odd = fs.bytes % 2;
      qlonglong half = fs.bytes / 2;

      if (half <= getSettingsValue("httpftp/minsegsize").toInt()) {
        // remove the desired urlIndex from the list of active URLs
        m_listActiveSegments.removeOne(urlIndex);
        return;
      }

      // notify the active thread of the change
      qlonglong from = fs.affectedClient->rangeFrom();
      qlonglong to = fs.affectedClient->rangeTo();
      if (to == -1) to = m_nTotal;
      fs.affectedClient->setRange(from, to = to - half);

      seg.offset = to;
      bytes = half;
    } else {
      // This should never happen
      return;
    }
  } else {
    // Find the first free spot smaller than seglim
    // Try not to create a new freeseg bigger than 5*seglim
    const int seglim = getSettingsValue("httpftp/minsegsize").toInt();

    for (int i = 0; i < m_segments.size(); i++) {
      if (m_segments[i].offset > lastEnd) {
        FreeSegment fs(lastEnd, m_segments[i].offset - lastEnd);
        if (i > 0 && m_segments[i - 1].client)
          fs.affectedClient = m_segments[i - 1].client;
        if (fs.bytes >= seglim || !fs.affectedClient) freeSegs << fs;
      }
      lastEnd = m_segments[i].offset + m_segments[i].bytes;
    }
    if (lastEnd < m_nTotal) {
      FreeSegment fs(lastEnd, m_nTotal - lastEnd);
      if (m_segments[m_segments.size() - 1].client)
        fs.affectedClient = m_segments[m_segments.size() - 1].client;
      qDebug() << "Lastseg affectedClient:" << fs.affectedClient;
      if (fs.bytes >= seglim || !fs.affectedClient) freeSegs << fs;
    }

    if (freeSegs.isEmpty()) return;  // This should never happen

    std::sort(freeSegs.begin(), freeSegs.end(), FreeSegment::compareByOffset);

    // Take the first one
    // If it's an allocated space, take it only if bytes >= seglim*5
    int bestSegment = 0;
    for (int i = 0; i < freeSegs.size(); i++) {
      if (freeSegs[i].bytes >= 5 * seglim || !freeSegs[i].affectedClient) {
        bestSegment = i;
        break;
      }
    }

    // Now try to be 5*seglim bytes far from the active client, if any
    FreeSegment& fs = freeSegs[bestSegment];
    qDebug() << "Best FreeSegment:" << freeSegs[bestSegment].offset
             << freeSegs[bestSegment].bytes;
    if (fs.affectedClient) {
      if (fs.bytes <= 5 * seglim) {
        qlonglong half = fs.bytes / 2;

        // notify the active thread of the change
        qlonglong from = fs.affectedClient->rangeFrom();
        qlonglong to = fs.affectedClient->rangeTo();
        if (to == -1) to = m_nTotal;
        fs.affectedClient->setRange(from, to = to - half);
        qDebug() << "New range of the pre-existing seg: " << from << to;

        seg.offset = to;
        bytes = half;
      } else {
        // 5*seglim far
        // notify the active thread of the change
        qlonglong from = fs.affectedClient->rangeFrom();
        qlonglong to = fs.affectedClient->rangeTo();
        if (to == -1) to = m_nTotal;
        fs.affectedClient->setRange(from, to = fs.offset + 5 * seglim);
        qDebug() << "New range of the pre-existing seg: " << from << to;
        qDebug() << "5*seglim:" << (5 * seglim);

        seg.offset = to;
        bytes = fs.bytes - 5 * seglim;
      }
    } else {
      seg.offset = freeSegs[bestSegment].offset;
      bytes = freeSegs[bestSegment].bytes;
    }
  }

  // start a new download thread
  qDebug() << "Start new seg: " << seg.offset << seg.offset + bytes;
  startSegment(seg, bytes);
  m_segments << seg;
}

void CurlDownload::stopSegment(int index, bool restarting) {
  Segment& s = m_segments[index];
  if (!s.client) return;
  updateSegmentProgress();
  s.urlIndex = -1;
  m_master->removeTransfer(s.client);
  s.client->stop();
  s.client = 0;
  simplifySegments(m_segments);

  bool hasActive = false;
  for (int i = 0; i < m_segments.size(); i++) {
    if (m_segments[i].client) {
      hasActive = true;
      break;
    }
  }
  if (!hasActive && !restarting) setState(Paused);
}

QColor CurlDownload::allocateSegmentColor() {
  QRandomGenerator rng;
  for (size_t i = 0; i < sizeof(g_colors) / sizeof(g_colors[0]); i++) {
    bool bFound = false;
    for (int j = 0; j < m_segments.size(); j++) {
      if (m_segments[j].client && m_segments[j].color == g_colors[i]) {
        bFound = true;
        break;
      }
    }

    if (!bFound) return g_colors[i];
  }

  return QColor(rng() % 256, rng() % 256, rng() % 256);
}

QObject* CurlDownload::createDetailsWidget(QWidget* w) {
  HttpDetails* d = new HttpDetails(w);
  d->setDownload(this);
  return d;
}

void CurlDownload::checkFileContents() {
#ifdef WITH_BITTORRENT
  if (getSettingsValue("httpftp/detect_torrents", true).toBool()) {
    QFile file(filePath());
    if (!file.open(QIODevice::ReadOnly)) return;

    char buf[11];
    if (file.read(buf, sizeof(buf)) != sizeof(buf)) return;

    if (memcmp(buf, "d8:announce", sizeof(buf)) == 0) {
      // Convert transfer to torrent
      TorrentDownload* t = new TorrentDownload;
      t->init(filePath(), m_dir.path());
      t->setState(state());

      file.remove();
      t->enterLogMessage(
          tr("This transfer was converted from a HTTP/FTP download"));
      replaceItself(t);
    }
  }
#endif
}
