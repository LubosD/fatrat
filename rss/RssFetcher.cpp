#include "RssFetcher.h"
#include "Logger.h"
#include "Queue.h"
#include "engines/TorrentDownload.h"
#include <QHttp>
#include <QSettings>
#include <QList>
#include <QBuffer>
#include <QUrl>
#include <QXmlSimpleReader>
#include <QtDebug>

extern QSettings* g_settings;
extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;

RssFetcher::RssFetcher()
	: m_bInItem(false), m_itemNextType(RssItem::None)
{
	m_timer.start(15*60*1000);
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(refresh()));
	refresh();
}

void RssFetcher::loadFeeds(QList<RssFeed>& items)
{
	int amount = g_settings->beginReadArray("rss/feeds");
	
	for(int i=0;i<amount;i++)
	{
		g_settings->setArrayIndex(i);
		RssFeed f = { g_settings->value("name").toString(), g_settings->value("url").toString(), -1 };
		items << f;
	}
	
	g_settings->endArray();
}

void RssFetcher::refresh()
{
	m_feeds.clear();
	
	loadFeeds(m_feeds);
	
	m_items.clear();
	
	for(int i=0;i<m_feeds.size();i++)
	{
		QHttp* http = new QHttp(this);
		QBuffer* buffer = new QBuffer(http);
		QUrl url = m_feeds[i].url;
		
		connect(http, SIGNAL(requestFinished(int,bool)), this, SLOT(requestFinished(int,bool)));
		
		buffer->open(QIODevice::ReadWrite);
		
		http->setHost(url.host(), url.port(80));
		m_feeds[i].request = http->get(url.path() + "?" + QString(url.encodedQuery()), buffer);
	}
}

void RssFetcher::requestFinished(int id, bool error)
{
	RssFeed* feed = 0;
	bool bLast = true;
	
	for(int i=0;i<m_feeds.size();i++)
	{
		if(m_feeds[i].request == id)
			feed = &m_feeds[i];
		if(m_feeds[i].request != id && m_feeds[i].request != -1)
			bLast = false;
	}
	
	if(!feed)
		return;
	
	if(!error)
	{
		QIODevice* device = static_cast<QHttp*>(sender())->currentDestinationDevice();
		
		device->seek(0);
		
		QXmlSimpleReader reader;
		QXmlInputSource source(device);
		
		reader.setContentHandler(this);
		
		m_strCurrentSource = feed->url;
		
		if(!reader.parse(&source))
			Logger::global()->enterLogMessage("RSS", tr("Failed to parse the feed \"%1\"").arg(feed->name));
	}
	else
	{
		Logger::global()->enterLogMessage("RSS", tr("Failed to fetch the feed \"%1\"").arg(feed->name));
	}
	
	feed->request = -1;
	
	if(bLast)
		processItems();
	
	sender()->deleteLater();
}

void RssFetcher::processItems()
{
	QReadLocker l(&g_queuesLock);
	QList<RssRegexp> regexps;
	QStringList newprocessed, processed = g_settings->value("rss/processed").toStringList();
	
	loadRegexps(regexps);
	
	foreach(RssItem item, m_items)
	{
		if(!processed.contains(item.url))
			processItem(regexps, item);
		newprocessed << item.url;
	}
	
	g_settings->setValue("rss/processed", newprocessed);
}

void RssFetcher::processItem(QList<RssRegexp>& regexps, const RssItem& item)
{
	for(int i=0;i<regexps.size();i++)
	{
		if(regexps[i].regexp.indexIn(item.title) != -1 && item.source == regexps[i].source)
		{
			// add a transfer
			Logger::global()->enterLogMessage("RSS", tr("Automatically adding a new BitTorrent transfer: %1").arg(item.title));
			Transfer* t = new TorrentDownload;
			
			t->init(item.url, regexps[i].target);
			t->setState(Transfer::Waiting);
			g_queues[regexps[i].queueIndex]->add(t);
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
	int count = g_settings->beginReadArray("rss/regexps");
	
	for(int i=0;i<count;i++)
	{
		RssRegexp item;
		g_settings->setArrayIndex(i);
		
		item.regexp = QRegExp(g_settings->value("regexp").toString(), Qt::CaseInsensitive);
		item.queueUUID = g_settings->value("queueUUID").toString();
		item.source = g_settings->value("source").toString();
		item.target = g_settings->value("target").toString();
		item.queueIndex = -1;
		
		items << item;
	}
	
	g_settings->endArray();
	
	updateRegexpQueues(items);
}

void RssFetcher::saveRegexps(const QList<RssRegexp>& items)
{
	g_settings->remove("rss/regexps");
	g_settings->beginWriteArray("rss/regexps", items.size());
	
	for(int i=0;i<items.size();i++)
	{
		g_settings->setArrayIndex(i);
		g_settings->setValue("regexp", items[i].regexp.pattern());
		g_settings->setValue("queueUUID", items[i].queueUUID);
		g_settings->setValue("source", items[i].source);
		g_settings->setValue("target", items[i].target);
	}
	
	g_settings->endArray();
}

void RssFetcher::saveFeeds(const QList<RssFeed>& items)
{
	g_settings->remove("rss/feeds");
	g_settings->beginWriteArray("rss/feeds", items.size());
	
	for(int i=0;i<items.size();i++)
	{
		g_settings->setArrayIndex(i);
		g_settings->setValue("name", items[i].name);
		g_settings->setValue("url", items[i].url);
	}
	
	g_settings->endArray();
}

bool RssFetcher::startElement(const QString& namespaceURI, const QString& localName, const QString& qName, const QXmlAttributes& atts)
{
	if(localName == "item")
	{
		m_bInItem = true;
		m_itemNext.source = m_strCurrentSource;
	}
	else if(m_bInItem)
	{
		if(localName == "title")
			m_itemNextType = RssItem::Title;
		else if(localName == "description")
			m_itemNextType = RssItem::Descr;
		else if(localName == "link")
			m_itemNextType = RssItem::Url;
		else
			m_itemNextType = RssItem::None;
	}
	else
		m_itemNextType = RssItem::None;
	return true;
}

bool RssFetcher::endElement(const QString& namespaceURI, const QString& localName, const QString& qName)
{
	if(localName == "item")
	{
		m_items << m_itemNext;
		m_itemNext = RssItem();
		m_bInItem = false;
	}
	
	m_itemNextType = RssItem::None;
	return true;
}

bool RssFetcher::characters(const QString& ch)
{
	if(m_itemNextType == RssItem::Descr)
		m_itemNext.descr += ch;
	else if(m_itemNextType == RssItem::Title)
		m_itemNext.title += ch;
	else if(m_itemNextType == RssItem::Url)
		m_itemNext.url += ch;
	
	return true;
}

