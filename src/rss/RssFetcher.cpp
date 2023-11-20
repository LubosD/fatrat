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

#include "RssFetcher.h"

#include <QBuffer>
#include <QDir>
#include <QList>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>
#include <QXmlSimpleReader>
#include <QtDebug>

#include "Logger.h"
#include "Queue.h"
#include "Settings.h"
#include "Transfer.h"

extern QSettings* g_settings;
extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;

RssFetcher* RssFetcher::m_instance = 0;

RssFetcher::RssFetcher() : m_bInItem(false), m_itemNextType(RssItem::None) {
  m_instance = this;
  connect(&m_timer, SIGNAL(timeout()), this, SLOT(refresh()));
  connect(&m_network, SIGNAL(finished(QNetworkReply*)), this,
          SLOT(requestFinished(QNetworkReply*)));
  applySettings();
}

RssFetcher::~RssFetcher() { m_instance = 0; }

void RssFetcher::applySettings() {
  enable(getSettingsValue("rss/enable").toBool());
}

void RssFetcher::enable(bool bEnable) {
  const int interval = getSettingsValue("rss/interval").toInt();
  bool bActive = m_timer.isActive();

  if (bActive != bEnable) {
    if (bEnable) {
      m_timer.start(interval * 60 * 1000);
      refresh();
    } else
      m_timer.stop();
  } else if (bEnable) {
    m_timer.start(interval * 60 * 1000);
  }
}

void RssFetcher::loadFeeds(QList<RssFeed>& items) {
  int amount = g_settings->beginReadArray("rss/feeds");

  for (int i = 0; i < amount; i++) {
    g_settings->setArrayIndex(i);
    RssFeed f = {g_settings->value("name").toString(),
                 g_settings->value("url").toString(), 0};
    items << f;
  }

  g_settings->endArray();
}

void RssFetcher::refresh() {
  m_feeds.clear();
  loadFeeds(m_feeds);
  m_items.clear();

  m_bAllOK = true;

  for (int i = 0; i < m_feeds.size(); i++) {
    QNetworkRequest request;

    request.setUrl(m_feeds[i].url);
    m_feeds[i].reply = m_network.get(request);
  }
}

void RssFetcher::requestFinished(QNetworkReply* reply) {
  RssFeed* feed = 0;
  bool bLast = true;

  for (int i = 0; i < m_feeds.size(); i++) {
    if (m_feeds[i].reply == reply) feed = &m_feeds[i];
    if (m_feeds[i].reply != reply && m_feeds[i].reply != 0) bLast = false;
  }

  if (!feed) return;

  if (reply->error() == QNetworkReply::NoError) {
    QIODevice* device = reply;

    device->seek(0);

    QXmlSimpleReader reader;
    QXmlInputSource source(device);

    reader.setErrorHandler(this);
    reader.setContentHandler(this);

    m_bInItem = false;
    m_strCurrentSource = feed->url;
    m_itemNext = RssItem();

    if (!reader.parse(&source)) {
      qDebug() << "Parse error:"
               << static_cast<QXmlErrorHandler*>(this)->errorString();
      Logger::global()->enterLogMessage(
          "RSS", tr("Failed to parse the feed \"%1\"").arg(feed->name));

      m_bAllOK = false;
    }
  } else {
    Logger::global()->enterLogMessage(
        "RSS", tr("Failed to fetch the feed \"%1\"").arg(feed->name));
    m_bAllOK = false;
  }

  feed->reply = 0;

  if (bLast) processItems();

  sender()->deleteLater();
}

void RssFetcher::processItems() {
  QReadLocker l(&g_queuesLock);
  QList<RssRegexp> regexps;
  QStringList newprocessed,
      processed = g_settings->value("rss/processed").toStringList();

  loadRegexps(regexps);

  if (!m_bAllOK) newprocessed << processed;

  foreach (RssItem item, m_items) {
    if (!processed.contains(item.url)) processItem(regexps, item);
    newprocessed << item.url;
  }

  saveRegexps(regexps);

  g_settings->setValue("rss/processed", newprocessed);
}

void RssFetcher::processItem(QList<RssRegexp>& regexps, const RssItem& item) {
  for (int i = 0; i < regexps.size(); i++) {
    if (regexps[i].regexp.indexIn(item.title) != -1 &&
        item.source == regexps[i].source) {
      if (regexps[i].tvs != RssRegexp::None) {
        QString epid = generateEpisodeName(regexps[i], item.title);
        qDebug() << "Generated an episode name" << epid << "from" << item.title;
        if (!epid.isEmpty()) {
          if (regexps[i].epDone.contains(epid)) continue;
          if (epid < regexps[i].from || epid > regexps[i].to) continue;
          regexps[i].epDone << epid;
        } else
          continue;
      }

      QStringList urls;

      if (!regexps[i].linkRegexp.isEmpty()) {
        int index = 0;
        while ((index = regexps[i].linkRegexp.indexIn(item.descr, index)) !=
               -1) {
          QString link = regexps[i].linkRegexp.cap(0);
          urls << link;
          index += link.size();
        }
      } else
        urls << item.url;

      foreach (QString url, urls) {
        // add a transfer
        Transfer::BestEngine eng;
        eng = Transfer::bestEngine(url, Transfer::Download);

        if (eng.nClass < 0)
          Logger::global()->enterLogMessage(
              "RSS", tr("The transfer wasn't accepted by any class: %1 - %2")
                         .arg(item.title)
                         .arg(url));
        else {
          Logger::global()->enterLogMessage(
              "RSS", tr("Automatically adding a new transfer: %1 - %2")
                         .arg(item.title)
                         .arg(item.url));
          Transfer* t = eng.engine->lpfnCreate();

          QDir().mkpath(regexps[i].target);

          t->init(url, regexps[i].target);
          t->setComment(item.descr);
          t->setState((regexps[i].addPaused) ? Transfer::Paused
                                             : Transfer::Waiting);
          g_queues[regexps[i].queueIndex]->add(t);
        }
      }
    }
  }
}

void RssFetcher::updateRegexpQueues(QList<RssRegexp>& items) {
  for (int i = 0; i < items.size(); i++) {
    for (int j = 0; j < g_queues.size(); j++) {
      if (g_queues[j]->uuid() == items[i].queueUUID) {
        items[i].queueIndex = j;
        break;
      }
    }

    if (items[i].queueIndex < 0) items.removeAt(i--);
  }
}

void RssFetcher::loadRegexps(QList<RssRegexp>& items) {
  int count = g_settings->beginReadArray("rss/regexps");

  for (int i = 0; i < count; i++) {
    RssRegexp item;
    g_settings->setArrayIndex(i);

    item.regexp =
        QRegExp(g_settings->value("regexp").toString(), Qt::CaseInsensitive);
    item.queueUUID = g_settings->value("queueUUID").toString();
    item.source = g_settings->value("source").toString();
    item.target = g_settings->value("target").toString();
    item.tvs = RssRegexp::TVSType(g_settings->value("tvs").toInt());
    item.epDone = g_settings->value("epdone").toStringList();
    item.from = g_settings->value("epfrom").toString();
    item.to = g_settings->value("epto").toString();
    item.includeTrailers = g_settings->value("includeTrailers").toBool();
    item.includeRepacks = g_settings->value("includeRepacks").toBool();
    item.excludeManuals = g_settings->value("excludeManuals").toBool();
    item.queueIndex = -1;
    item.addPaused = g_settings->value("addPaused").toBool();
    item.linkRegexp = QRegExp(g_settings->value("linkRegexp").toString(),
                              Qt::CaseInsensitive);

    items << item;
  }

  g_settings->endArray();

  updateRegexpQueues(items);
}

void RssFetcher::saveRegexps(const QList<RssRegexp>& items) {
  g_settings->remove("rss/regexps");
  g_settings->beginWriteArray("rss/regexps", items.size());

  for (int i = 0; i < items.size(); i++) {
    g_settings->setArrayIndex(i);
    g_settings->setValue("regexp", items[i].regexp.pattern());
    g_settings->setValue("queueUUID", items[i].queueUUID);
    g_settings->setValue("source", items[i].source);
    g_settings->setValue("target", items[i].target);
    g_settings->setValue("tvs", items[i].tvs);
    g_settings->setValue("epdone", items[i].epDone);
    g_settings->setValue("epfrom", items[i].from);
    g_settings->setValue("epto", items[i].to);
    g_settings->setValue("includeTrailers", items[i].includeTrailers);
    g_settings->setValue("includeRepacks", items[i].includeRepacks);
    g_settings->setValue("excludeManuals", items[i].excludeManuals);
    g_settings->setValue("addPaused", items[i].addPaused);
    g_settings->setValue("linkRegexp", items[i].linkRegexp.pattern());
  }

  g_settings->endArray();
}

void RssFetcher::saveFeeds(const QList<RssFeed>& items) {
  g_settings->remove("rss/feeds");
  g_settings->beginWriteArray("rss/feeds", items.size());

  for (int i = 0; i < items.size(); i++) {
    g_settings->setArrayIndex(i);
    g_settings->setValue("name", items[i].name);
    g_settings->setValue("url", items[i].url);
  }

  g_settings->endArray();
}

void RssFetcher::performManualCheck(QString torrentName) {
  QList<RssRegexp> re;
  bool bModified = false;

  loadRegexps(re);

  for (int i = 0; i < re.size(); i++) {
    if (!re[i].excludeManuals || re[i].regexp.indexIn(torrentName) < 0)
      continue;
    QString episode = generateEpisodeName(re[i], torrentName);

    if (!episode.isEmpty() && !re[i].epDone.contains(episode)) {
      bModified = true;
      re[i].epDone << episode;
    }
  }

  if (bModified) saveRegexps(re);
}

QString RssFetcher::generateEpisodeName(const RssRegexp& match,
                                        QString itemName) {
  QString rval;
  QChar zero('0');

  if (match.tvs == RssRegexp::None)
    return QString();
  else if (match.tvs == RssRegexp::SeasonBased) {
    QRegExp matcher1("(\\d+)x(\\d+)"), matcher2("S(\\d+)E(\\d+)");
    if (matcher1.indexIn(itemName) != -1)
      rval = QString("S%1E%2")
                 .arg(matcher1.cap(1).toInt(), 2, 10, zero)
                 .arg(matcher1.cap(2).toInt(), 2, 10, zero);
    else if (matcher2.indexIn(itemName) != -1)
      rval = QString("S%1E%2")
                 .arg(matcher2.cap(1).toInt(), 2, 10, zero)
                 .arg(matcher2.cap(2).toInt(), 2, 10, zero);
  } else if (match.tvs == RssRegexp::EpisodeBased) {
    QRegExp matcher("\\d+");
    if (matcher.indexIn(itemName) != -1)
      rval = QString("%1").arg(matcher.cap(1).toInt(), 4, 10, zero);
  } else if (match.tvs == RssRegexp::DateBased) {
    QRegExp matcher1("(\\d{4})[\\-\\. ](\\d\\d)[\\-\\. ](\\d\\d)"),
        matcher2("(\\d\\d)[\\-\\. ](\\d\\d)[\\-\\. ](\\d{2,4})"),
        matcher3("(\\d\\d?)[\\-\\. ](\\w{3,})[\\-\\. ](\\d{2,4})");
    if (matcher1.indexIn(itemName) != -1) {
      int month = matcher1.cap(2).toInt(), day = matcher1.cap(3).toInt();
      dayMonthHeuristics(day, month);
      rval = QString("%1-%2-%3")
                 .arg(matcher1.cap(1).toInt())
                 .arg(month, 2, 10, zero)
                 .arg(day, 2, 10, zero);
    } else if (matcher2.indexIn(itemName) != -1) {
      int year = matcher2.cap(3).toInt();
      if (year < 100) year += 2000;
      int day = matcher2.cap(1).toInt();
      int month = matcher2.cap(2).toInt();

      dayMonthHeuristics(day, month);
      rval = QString("%1-%2-%3")
                 .arg(year)
                 .arg(month, 2, 10, zero)
                 .arg(day, 2, 10, zero);
    } else if (matcher3.indexIn(itemName) != -1) {
      const char* months[] = {"January",   "February", "March",    "April",
                              "May",       "June",     "July",     "August",
                              "September", "October",  "November", "December"};
      int month = 0;
      int year = matcher3.cap(3).toInt();
      if (year < 100) year += 2000;

      QString m = matcher3.cap(2);
      for (int i = 0; i < 12; i++) {
        QString sshort = months[i];
        sshort.resize(3);

        if (!m.compare(months[i], Qt::CaseInsensitive) ||
            !m.compare(sshort, Qt::CaseInsensitive)) {
          month = i + 1;
          break;
        }
      }

      if (month) {
        rval = QString("%1-%2-%3")
                   .arg(year)
                   .arg(month, 2, 10, zero)
                   .arg(matcher3.cap(1).toInt(), 2, 10, zero);
      }
    }
  }

  if (!rval.isEmpty()) {
    if (itemName.indexOf("trailer", -1, Qt::CaseInsensitive) != -1 ||
        itemName.indexOf("teaser", -1, Qt::CaseInsensitive) != -1) {
      if (match.includeTrailers)
        rval += "|trailer";
      else
        rval.clear();
    }
    if (itemName.indexOf("repack", -1, Qt::CaseInsensitive) != -1) {
      if (match.includeRepacks) rval += "|repack";
    }
    if (itemName.indexOf("proper", -1, Qt::CaseInsensitive) != -1) {
      if (match.includeRepacks) rval += "|proper";
    }
  }

  return rval;
}

void RssFetcher::dayMonthHeuristics(int& day, int& month) {
  // Some Americans are complete idiots when it comes to writing dates
  // Hence we have to do some guessing to recognize which field is the month and
  // which one is the day
  QDate date = QDate::currentDate();

  if (month > 12)
    std::swap(month, day);
  else {
    int prevmonth = date.month() - 1;
    if (!prevmonth) prevmonth = 12;

    if (month != date.month() && month != prevmonth &&
        (day == date.month() || day == prevmonth))
      std::swap(month, day);
  }
}

bool RssFetcher::startElement(const QString& namespaceURI,
                              const QString& localName, const QString& qName,
                              const QXmlAttributes& atts) {
  if (localName == "item") {
    m_bInItem = true;
    m_itemNext.source = m_strCurrentSource;
  } else if (m_bInItem) {
    if (localName == "title")
      m_itemNextType = RssItem::Title;
    else if (localName == "description")
      m_itemNextType = RssItem::Descr;
    else if (localName == "link" && m_itemNext.url.isEmpty())
      m_itemNextType = RssItem::Url;
    else if (localName == "enclosure") {
      m_itemNext.url = atts.value("url");
      m_itemNextType = RssItem::None;
    } else
      m_itemNextType = RssItem::None;
  } else
    m_itemNextType = RssItem::None;
  return true;
}

bool RssFetcher::endElement(const QString& namespaceURI,
                            const QString& localName, const QString& qName) {
  if (localName == "item") {
    m_items << m_itemNext;
    m_itemNext = RssItem();
    m_bInItem = false;
  }

  m_itemNextType = RssItem::None;
  return true;
}

bool RssFetcher::characters(const QString& ch) {
  if (m_bInItem) {
    if (m_itemNextType == RssItem::Descr)
      m_itemNext.descr += ch;
    else if (m_itemNextType == RssItem::Title)
      m_itemNext.title += ch;
    else if (m_itemNextType == RssItem::Url)
      m_itemNext.url += ch;
  }

  return true;
}
