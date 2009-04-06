/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation
with the OpenSSL special exemption.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef RSSFETCHER_H
#define RSSFETCHER_H
#include <QObject>
#include <QTimer>
#include <QXmlDefaultHandler>
#include <QRegExp>

struct RssFeed
{
	QString name, url;
	int request;
};

struct RssItem
{
	QString title, descr, url;
	QString source; // RSS URL
	QString id; // TV show episode ID
	enum Next { None, Title, Descr, Url };
};

struct RssRegexp
{
	QString source, target;
	QRegExp regexp;
	QString queueUUID, from, to;
	QStringList epDone;
	int queueIndex;
	bool excludeManuals, includeTrailers, includeRepacks;
	
	enum TVSType { None = 0, SeasonBased, EpisodeBased, DateBased } tvs;
};

class RssFetcher : public QObject, public QXmlDefaultHandler
{
Q_OBJECT
public:
	RssFetcher();
	~RssFetcher();
	static RssFetcher* instance() { return m_instance; }
	
	bool startElement(const QString& namespaceURI, const QString& localName, const QString& qName, const QXmlAttributes& atts);
	bool endElement(const QString& namespaceURI, const QString& localName, const QString& qName);
	bool characters(const QString& ch);
	void processItems();
	
	void applySettings();
	void enable(bool bEnable);
	
	static void processItem(QList<RssRegexp>& regexps, const RssItem& item);
	
	static void performManualCheck(QString torrentName);
	
	static void updateRegexpQueues(QList<RssRegexp>& items); // g_queuesLock ought to be locked
	static void loadRegexps(QList<RssRegexp>& items);
	static void loadFeeds(QList<RssFeed>& items);
	
	static void saveRegexps(const QList<RssRegexp>& items);
	static void saveFeeds(const QList<RssFeed>& items);
	
	static QString generateEpisodeName(const RssRegexp& match, QString itemName);
	static void dayMonthHeuristics(int& day, int& month);
public slots:
	void refresh();
	void requestFinished(int id, bool error);
private:
	static RssFetcher* m_instance;
	
	QTimer m_timer;
	QList<RssFeed> m_feeds;
	QList<RssItem> m_items;
	bool m_bAllOK;
	
	// XML parsing
	bool m_bInItem;
	RssItem m_itemNext;
	RssItem::Next m_itemNextType;
	QString m_strCurrentSource;
};

#endif
