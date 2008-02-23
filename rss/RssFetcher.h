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
public slots:
	void refresh();
	void requestFinished(int id, bool error);
private:
	QTimer m_timer;
	QList<RssFeed> m_feeds;
	QList<RssItem> m_items;
	
	// XML parsing
	bool m_bInItem;
	RssItem m_itemNext;
	RssItem::Next m_itemNextType;
	QString m_strCurrentSource;
};

#endif
