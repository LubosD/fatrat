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

#ifndef RSSFETCHER_H
#define RSSFETCHER_H
#include <QObject>
#include <QTimer>
#include <QRegularExpression>
#include <QNetworkAccessManager>
#include "RssFeedParser.h"

struct RssFeed
{
	QString id, name, url;
	QNetworkReply* reply = 0;
};

struct RssRegexp
{
	QString id, source, target;
	QRegularExpression regexp;
	QString queueUUID, from, to;
	QStringList epDone;
	QRegularExpression linkRegexp;
	int queueIndex = -1;
	bool excludeManuals = true, includeTrailers = false, includeRepacks = false, addPaused = false;
	
	enum TVSType { None = 0, SeasonBased, EpisodeBased, DateBased } tvs = None;
};

class RssFetcher : public QObject
{
Q_OBJECT
public:
	RssFetcher();
	~RssFetcher();
	static RssFetcher* instance() { return m_instance; }
	
	void processItems();
	
	void applySettings();
	void enable(bool bEnable);
	
	static void processItem(QList<RssRegexp>& regexps, const RssItem& item);
	
	static void performManualCheck(QString torrentName);
	
	static void updateRegexpQueues(QList<RssRegexp>& items); // g_queuesLock ought to be locked
	static void loadRegexps(QList<RssRegexp>& items);
	// the same, but keeps the regexps whose queue no longer exists (queueIndex stays -1)
	static void loadRegexpsUnfiltered(QList<RssRegexp>& items);
	static void loadFeeds(QList<RssFeed>& items);
	
	static void saveRegexps(const QList<RssRegexp>& items);
	static void saveFeeds(const QList<RssFeed>& items);
	
	static QString generateEpisodeName(const RssRegexp& match, QString itemName);
	static void dayMonthHeuristics(int& day, int& month);
	// whether id is a well-formed entry of RssRegexp::epDone for the given tracking mode
	static bool isValidEpisodeId(RssRegexp::TVSType tvs, const QString& id);
	static QString newId();
public slots:
	void refresh();
	void requestFinished(QNetworkReply*);
private:
	static RssFetcher* m_instance;
	
	QTimer m_timer;
	QList<RssFeed> m_feeds;
	QList<RssItem> m_items;
	bool m_bAllOK;
	
	QNetworkAccessManager m_network;
};

#endif
