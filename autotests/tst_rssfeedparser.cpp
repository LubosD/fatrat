#include <QtTest>
#include <QBuffer>
#include "rss/RssFeedParser.h"

class TestRssFeedParser : public QObject
{
	Q_OBJECT
private slots:
	void titleLinkDescription();
	void cdataAndEnclosure();
	void firstEnclosureLinkWins();
	void firstLinkWins();
	void namespacedTagsAreMatchedByLocalName();
	void markupInsideDescriptionDropsTheTail();
	void undefinedEntitiesAreDropped();
	void itemsBeforeAParseErrorAreKept();
	void emptyInputIsAnError();
	void feedWithoutItems();
};

static bool parse(const QString& xml, QList<RssItem>& items)
{
	QByteArray data = xml.toUtf8();
	QBuffer buffer(&data);
	buffer.open(QIODevice::ReadOnly);
	return parseRssFeed(&buffer, "SRC", items);
}

void TestRssFeedParser::titleLinkDescription()
{
	QList<RssItem> items;
	QVERIFY(parse("<rss><channel><title>chan</title><item><title>Show.S01E02</title>"
		"<link>http://a/b</link><description>d &amp; e</description></item></channel></rss>", items));
	QCOMPARE(items.size(), 1);
	QCOMPARE(items[0].title, QString("Show.S01E02"));
	QCOMPARE(items[0].url, QString("http://a/b"));
	QCOMPARE(items[0].descr, QString("d & e"));
	QCOMPARE(items[0].source, QString("SRC"));
}

void TestRssFeedParser::cdataAndEnclosure()
{
	QList<RssItem> items;
	QVERIFY(parse("<rss><item><title><![CDATA[Cdata <b> title]]></title>"
		"<enclosure url=\"http://x/y.torrent\" length=\"1\"/><link>ignored</link></item></rss>", items));
	QCOMPARE(items.size(), 1);
	QCOMPARE(items[0].title, QString("Cdata <b> title"));
	QCOMPARE(items[0].url, QString("http://x/y.torrent"));
}

void TestRssFeedParser::firstEnclosureLinkWins()
{
	QList<RssItem> items;
	QVERIFY(parse("<rss><item><title>a</title><enclosure url=\"http://e1\"/><enclosure url=\"http://e2\"/></item></rss>", items));
	QCOMPARE(items[0].url, QString("http://e2"));
}

void TestRssFeedParser::firstLinkWins()
{
	QList<RssItem> items;
	QVERIFY(parse("<rss><item><link>http://l/1</link><link>http://l/2</link></item></rss>", items));
	QCOMPARE(items[0].url, QString("http://l/1"));
}

void TestRssFeedParser::namespacedTagsAreMatchedByLocalName()
{
	QList<RssItem> items;
	QVERIFY(parse("<rss xmlns:media=\"http://m\"><item><media:title>ns title</media:title><title>real</title></item></rss>", items));
	QCOMPARE(items[0].title, QString("ns titlereal"));
}

void TestRssFeedParser::markupInsideDescriptionDropsTheTail()
{
	QList<RssItem> items;
	QVERIFY(parse("<rss><item><description>a <b>bold</b> tail</description></item></rss>", items));
	QCOMPARE(items[0].descr, QString("a "));
}

void TestRssFeedParser::undefinedEntitiesAreDropped()
{
	QList<RssItem> items;
	QVERIFY(parse("<rss><item><title>x&nbsp;y</title></item></rss>", items));
	QCOMPARE(items[0].title, QString("xy"));
}

void TestRssFeedParser::itemsBeforeAParseErrorAreKept()
{
	QList<RssItem> items;
	QVERIFY(!parse("<rss><channel><item><title>first</title></item><item><title>trunc", items));
	QCOMPARE(items.size(), 1);
	QCOMPARE(items[0].title, QString("first"));
}

void TestRssFeedParser::emptyInputIsAnError()
{
	QList<RssItem> items;
	QVERIFY(!parse("", items));
	QVERIFY(items.isEmpty());
}

void TestRssFeedParser::feedWithoutItems()
{
	QList<RssItem> items;
	QVERIFY(parse("<rss><channel><title>no items</title></channel></rss>", items));
	QVERIFY(items.isEmpty());
}

QTEST_APPLESS_MAIN(TestRssFeedParser)
#include "tst_rssfeedparser.moc"
