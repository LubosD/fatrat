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

#include "Settings.h"
#include "RssFetcher.h"
#include "Logger.h"
#include "Queue.h"
#include "Transfer.h"
#include <QSettings>
#include <QList>
#include <QBuffer>
#include <QUrl>
#include <QDir>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QtDebug>
#include "util/LegacyRegex.h"

extern QSettings* g_settings;
extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;

RssFetcher* RssFetcher::m_instance = 0;

RssFetcher::RssFetcher()
{
	m_instance = this;
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(refresh()));
	connect(&m_network, SIGNAL(finished(QNetworkReply*)), this, SLOT(requestFinished(QNetworkReply*)));
	applySettings();
}

RssFetcher::~RssFetcher()
{
	m_instance = 0;
}

void RssFetcher::applySettings()
{
	enable(getSettingsValue("rss/enable").toBool());
}

void RssFetcher::enable(bool bEnable)
{
	const int interval = getSettingsValue("rss/interval").toInt();
	bool bActive = m_timer.isActive();
	
	if(bActive != bEnable)
	{
		if(bEnable)
		{
			m_timer.start(interval*60*1000);
			refresh();
		}
		else
			m_timer.stop();
	}
	else if(bEnable)
	{
		m_timer.start(interval*60*1000);
	}
}

void RssFetcher::refresh()
{
	m_feeds.clear();
	loadFeeds(m_feeds);
	m_items.clear();
	
	m_bAllOK = true;
	
	for(int i=0;i<m_feeds.size();i++)
	{
		QNetworkRequest request;

		request.setUrl(m_feeds[i].url);
		m_feeds[i].reply = m_network.get(request);
	}
}

void RssFetcher::requestFinished(QNetworkReply* reply)
{
	RssFeed* feed = 0;
	bool bLast = true;
	
	for(int i=0;i<m_feeds.size();i++)
	{
		if(m_feeds[i].reply == reply)
			feed = &m_feeds[i];
		if(m_feeds[i].reply != reply && m_feeds[i].reply != 0)
			bLast = false;
	}
	
	if(!feed)
		return;
	
	if(reply->error() == QNetworkReply::NoError)
	{
		QIODevice* device = reply;
		
		device->seek(0);
		
		QString error;
		
		if(!parseRssFeed(device, feed->url, m_items, &error))
		{
			qDebug() << "Parse error:" << error;
			Logger::global()->enterLogMessage("RSS", tr("Failed to parse the feed \"%1\"").arg(feed->name));

			m_bAllOK = false;
		}
	}
	else
	{
		Logger::global()->enterLogMessage("RSS", tr("Failed to fetch the feed \"%1\"").arg(feed->name));
		m_bAllOK = false;
	}
		
	
	feed->reply = 0;
	
	if(bLast)
		processItems();
	
	reply->deleteLater();
}

void RssFetcher::processItems()
{
	QReadLocker l(&g_queuesLock);
	QList<RssRegexp> regexps;
	QStringList newprocessed, processed = g_settings->value("rss/processed").toStringList();
	
	loadRegexps(regexps);
	
	if(!m_bAllOK)
		newprocessed << processed;
	
	foreach(RssItem item, m_items)
	{
		if(!processed.contains(item.url))
			processItem(regexps, item);
		newprocessed << item.url;
	}
	
	saveRegexps(regexps);
	
	g_settings->setValue("rss/processed", newprocessed);
}

void RssFetcher::processItem(QList<RssRegexp>& regexps, const RssItem& item)
{
	for(int i=0;i<regexps.size();i++)
	{
		if(regexps[i].regexp.match(item.title).hasMatch() && item.source == regexps[i].source)
		{
			if(regexps[i].tvs != RssRegexp::None)
			{
				QString epid = generateEpisodeName(regexps[i], item.title);
				qDebug() << "Generated an episode name" << epid << "from" << item.title;
				if(!epid.isEmpty())
				{
					if(regexps[i].epDone.contains(epid))
						continue;
					if(epid < regexps[i].from || epid > regexps[i].to)
						continue;
					regexps[i].epDone << epid;
				}
				else
					continue;
			}
			
			QStringList urls;

			if(!regexps[i].linkRegexp.pattern().isEmpty())
			{
				QRegularExpressionMatchIterator it = regexps[i].linkRegexp.globalMatch(item.descr);
				while(it.hasNext())
					urls << it.next().captured(0);
			}
			else if(!item.magnetUri.trimmed().isEmpty())
				urls << item.magnetUri.trimmed();
			else
				urls << item.url;

			foreach(QString url, urls)
			{
				// add a transfer
				Transfer::BestEngine eng;
				eng = Transfer::bestEngine(url, Transfer::Download);

				if(eng.nClass < 0)
					Logger::global()->enterLogMessage("RSS", tr("The transfer wasn't accepted by any class: %1 - %2").arg(item.title).arg(url));
				else
				{
					Logger::global()->enterLogMessage("RSS", tr("Automatically adding a new transfer: %1 - %2").arg(item.title).arg(url));
					Transfer* t = eng.engine->lpfnCreate();

					QDir().mkpath(regexps[i].target);

					t->init(url, regexps[i].target);
					t->setComment(item.descr);
					t->setState((regexps[i].addPaused) ? Transfer::Paused : Transfer::Waiting);
					g_queues[regexps[i].queueIndex]->add(t);
				}
			}
		}
	}
}

void RssFetcher::updateRegexpQueues(QList<RssRegexp>& items)
{
	for(int i=0;i<items.size();i++)
	{
		for(int j=0;j<g_queues.size();j++)
		{
			if(g_queues[j]->uuid() == items[i].queueUUID)
			{
				items[i].queueIndex = j;
				break;
			}
		}
		
		if(items[i].queueIndex < 0)
			items.removeAt(i--);
	}
}

void RssFetcher::loadRegexps(QList<RssRegexp>& items)
{
	loadRegexpsUnfiltered(items);
	updateRegexpQueues(items);
}

void RssFetcher::performManualCheck(QString torrentName)
{
	QList<RssRegexp> re;
	bool bModified = false;
	
	loadRegexps(re);
	
	for(int i=0;i<re.size();i++)
	{
		if(!re[i].excludeManuals || !re[i].regexp.match(torrentName).hasMatch())
			continue;
		QString episode = generateEpisodeName(re[i], torrentName);
		
		if(!episode.isEmpty() && !re[i].epDone.contains(episode))
		{
			bModified = true;
			re[i].epDone << episode;
		}
	}
	
	if(bModified)
		saveRegexps(re);
}

