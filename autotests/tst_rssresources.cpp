#include <QtTest>
#include <QSettings>
#include <QTemporaryDir>
#include "remote/RssJson.h"

QSettings* g_settings = nullptr;

class TestRssResources : public QObject
{
	Q_OBJECT
private slots:
	void initTestCase();
	void cleanup();

	void episodeSeason();
	void episodeBasedUsesTheNumber();
	void episodeDate();
	void trailerAndRepackFlags();
	void validEpisodeIds();

	void idsAreGeneratedAndPersisted();
	void idsSurviveSaveAndLoad();
	void unfilteredLoadKeepsEveryRegexp();

	void feedRequiresNameAndUrl();
	void regexpCreateDefaultsAndValidation();
	void regexpModeChangeResetsRange();
	void regexpPatchIsPartial();
	void testEndpointMatchesTheEngine();
	void downloadedEpisodesAreValidated();
private:
	QTemporaryDir m_tmp;
};

static bool queues(const QString& uuid, QString* dir)
{
	if (uuid != "q1")
		return false;
	*dir = "/downloads";
	return true;
}

static int failure(const std::function<void()>& fn)
{
	try { fn(); }
	catch (const RestError& e) { return e.status; }
	return 0;
}

static QList<RssFeed> oneFeed()
{
	RssFeed f;
	f.id = "f1";
	f.name = "Feed";
	f.url = "http://example/rss";
	return { f };
}

void TestRssResources::initTestCase()
{
	QVERIFY(m_tmp.isValid());
}

void TestRssResources::cleanup()
{
	delete g_settings;
	g_settings = new QSettings(m_tmp.filePath("test.ini"), QSettings::IniFormat);
	g_settings->clear();
}

static RssRegexp mode(RssRegexp::TVSType tvs)
{
	RssRegexp re;
	re.tvs = tvs;
	return re;
}

void TestRssResources::episodeSeason()
{
	QCOMPARE(RssFetcher::generateEpisodeName(mode(RssRegexp::SeasonBased), "Show.S1E2.720p"), QString("S01E02"));
	QCOMPARE(RssFetcher::generateEpisodeName(mode(RssRegexp::SeasonBased), "Show 3x14"), QString("S03E14"));
	QVERIFY(RssFetcher::generateEpisodeName(mode(RssRegexp::SeasonBased), "Show").isEmpty());
	QVERIFY(RssFetcher::generateEpisodeName(mode(RssRegexp::None), "Show.S01E02").isEmpty());
}

void TestRssResources::episodeBasedUsesTheNumber()
{
	// used to be 0000 for every title (capture group 1 of a group-less pattern)
	QCOMPARE(RssFetcher::generateEpisodeName(mode(RssRegexp::EpisodeBased), "Anime 123"), QString("0123"));
	QVERIFY(RssFetcher::generateEpisodeName(mode(RssRegexp::EpisodeBased), "Anime").isEmpty());
}

void TestRssResources::episodeDate()
{
	QCOMPARE(RssFetcher::generateEpisodeName(mode(RssRegexp::DateBased), "News 2026-10-16"), QString("2026-10-16"));
	QCOMPARE(RssFetcher::generateEpisodeName(mode(RssRegexp::DateBased), "News 16 Oct 2026"), QString("2026-10-16"));
}

void TestRssResources::trailerAndRepackFlags()
{
	RssRegexp re = mode(RssRegexp::SeasonBased);
	QVERIFY(RssFetcher::generateEpisodeName(re, "Show.S01E02.Trailer").isEmpty());
	re.includeTrailers = true;
	QCOMPARE(RssFetcher::generateEpisodeName(re, "Show.S01E02.Trailer"), QString("S01E02|trailer"));
	QCOMPARE(RssFetcher::generateEpisodeName(re, "Show.S01E02.REPACK"), QString("S01E02"));
	re.includeRepacks = true;
	QCOMPARE(RssFetcher::generateEpisodeName(re, "Show.S01E02.REPACK"), QString("S01E02|repack"));
}

void TestRssResources::validEpisodeIds()
{
	QVERIFY(RssFetcher::isValidEpisodeId(RssRegexp::SeasonBased, "S01E02"));
	QVERIFY(RssFetcher::isValidEpisodeId(RssRegexp::SeasonBased, "S01E02|repack|proper"));
	QVERIFY(!RssFetcher::isValidEpisodeId(RssRegexp::SeasonBased, "S1E2"));
	QVERIFY(!RssFetcher::isValidEpisodeId(RssRegexp::SeasonBased, "2026-10-16"));
	QVERIFY(RssFetcher::isValidEpisodeId(RssRegexp::EpisodeBased, "0123"));
	QVERIFY(!RssFetcher::isValidEpisodeId(RssRegexp::EpisodeBased, "12"));
	QVERIFY(RssFetcher::isValidEpisodeId(RssRegexp::DateBased, "2026-10-16|trailer"));
	QVERIFY(!RssFetcher::isValidEpisodeId(RssRegexp::DateBased, "2026-10-16|bogus"));
	QVERIFY(!RssFetcher::isValidEpisodeId(RssRegexp::None, "anything"));
}

void TestRssResources::idsAreGeneratedAndPersisted()
{
	// a configuration written before ids existed
	g_settings->beginWriteArray("rss/feeds", 1);
	g_settings->setArrayIndex(0);
	g_settings->setValue("name", "Old");
	g_settings->setValue("url", "http://old/rss");
	g_settings->endArray();
	g_settings->beginWriteArray("rss/regexps", 2);
	for (int i = 0; i < 2; i++)
	{
		g_settings->setArrayIndex(i);
		g_settings->setValue("regexp", QString("re%1").arg(i));
		g_settings->setValue("source", "http://old/rss");
		g_settings->setValue("queueUUID", "gone");
	}
	g_settings->endArray();

	QList<RssFeed> a, b;
	QList<RssRegexp> ra, rb;
	RssFetcher::loadFeeds(a);
	RssFetcher::loadFeeds(b);
	RssFetcher::loadRegexpsUnfiltered(ra);
	RssFetcher::loadRegexpsUnfiltered(rb);

	QVERIFY(!a[0].id.isEmpty());
	QCOMPARE(a[0].id, b[0].id);
	QVERIFY(!ra[0].id.isEmpty());
	QVERIFY(ra[0].id != ra[1].id);
	QCOMPARE(ra[0].id, rb[0].id);
	QCOMPARE(ra[1].id, rb[1].id);
	QCOMPARE(ra[1].regexp.pattern(), QString("re1"));
}

void TestRssResources::idsSurviveSaveAndLoad()
{
	QList<RssFeed> feeds = oneFeed();
	RssFetcher::saveFeeds(feeds);

	RssRegexp re;
	re.id = "r1";
	re.source = feeds[0].url;
	re.epDone << "S01E01";
	re.tvs = RssRegexp::SeasonBased;
	RssFetcher::saveRegexps({ re });

	QList<RssFeed> f2;
	QList<RssRegexp> r2;
	RssFetcher::loadFeeds(f2);
	RssFetcher::loadRegexpsUnfiltered(r2);
	QCOMPARE(f2[0].id, QString("f1"));
	QCOMPARE(r2[0].id, QString("r1"));
	QCOMPARE(r2[0].epDone, QStringList{"S01E01"});
	QCOMPARE(r2[0].tvs, RssRegexp::SeasonBased);

	// an item saved without an id gets one, and keeps it
	RssRegexp noId = re;
	noId.id.clear();
	RssFetcher::saveRegexps({ noId });
	RssFetcher::loadRegexpsUnfiltered(r2 = {});
	QVERIFY(!r2[0].id.isEmpty());
}

void TestRssResources::unfilteredLoadKeepsEveryRegexp()
{
	RssRegexp re;
	re.id = "r1";
	re.queueUUID = "no-such-queue";
	RssFetcher::saveRegexps({ re, re });

	QList<RssRegexp> list;
	RssFetcher::loadRegexpsUnfiltered(list);
	QCOMPARE(list.size(), 2);
	QCOMPARE(list[0].queueIndex, -1);
}

void TestRssResources::feedRequiresNameAndUrl()
{
	RssFeed f;
	QCOMPARE(failure([&] { RssJson::applyFeed(f, QJsonObject{{"name", "x"}}, true); }), 422);
	QCOMPARE(failure([&] { RssJson::applyFeed(f, QJsonObject{{"name", " "}, {"url", "u"}}, true); }), 422);
	QCOMPARE(failure([&] { RssJson::applyFeed(f, QJsonObject{{"name", 5}, {"url", "u"}}, true); }), 422);
	QCOMPARE(failure([&] { RssJson::applyFeed(f, QJsonObject{{"name", " Name "}, {"url", "http://u"}}, true); }), 0);
	QCOMPARE(f.name, QString("Name"));
	QCOMPARE(failure([&] { RssJson::applyFeed(f, QJsonObject{{"url", ""}}, false); }), 422);
	QCOMPARE(f.url, QString("http://u"));
}

void TestRssResources::regexpCreateDefaultsAndValidation()
{
	const QList<RssFeed> feeds = oneFeed();
	RssRegexp re;
	re.id = "r1";

	QCOMPARE(failure([&] { RssJson::applyRegexp(re, QJsonObject{{"queue", "q1"}}, feeds, queues, true); }), 422);
	QCOMPARE(failure([&] { RssJson::applyRegexp(re, QJsonObject{{"feedId", "nope"}, {"queue", "q1"}}, feeds, queues, true); }), 422);
	QCOMPARE(failure([&] { RssJson::applyRegexp(re, QJsonObject{{"feedId", "f1"}, {"queue", "nope"}}, feeds, queues, true); }), 422);
	QCOMPARE(failure([&] { RssJson::applyRegexp(re, QJsonObject{{"feedId", "f1"}, {"queue", "q1"}, {"expression", "("}}, feeds, queues, true); }), 422);
	QCOMPARE(failure([&] { RssJson::applyRegexp(re, QJsonObject{{"feedId", "f1"}, {"queue", "q1"}, {"linkRegexp", "["}}, feeds, queues, true); }), 422);
	QCOMPARE(failure([&] { RssJson::applyRegexp(re, QJsonObject{{"feedId", "f1"}, {"queue", "q1"}, {"tvs", "weekly"}}, feeds, queues, true); }), 422);
	QVERIFY(re.source.isEmpty());	// a rejected request changes nothing

	QCOMPARE(failure([&] { RssJson::applyRegexp(re, QJsonObject{{"feedId", "f1"}, {"queue", "q1"}, {"expression", "show"}}, feeds, queues, true); }), 0);
	QCOMPARE(re.source, feeds[0].url);
	QCOMPARE(re.target, QString("/downloads"));
	QCOMPARE(re.queueUUID, QString("q1"));
	QCOMPARE(re.tvs, RssRegexp::None);

	QJsonObject o = RssJson::regexpToJson(re, feeds);
	QCOMPARE(o["feedId"].toString(), QString("f1"));
	QCOMPARE(o["expression"].toString(), QString("show"));
	QCOMPARE(o["tvs"].toString(), QString("none"));
	QCOMPARE(o["id"].toString(), QString("r1"));
}

void TestRssResources::regexpModeChangeResetsRange()
{
	const QList<RssFeed> feeds = oneFeed();
	RssRegexp re;
	RssJson::applyRegexp(re, QJsonObject{{"feedId", "f1"}, {"queue", "q1"}, {"tvs", "season"}}, feeds, queues, true);
	QCOMPARE(re.from, QString("S00E00"));
	QCOMPARE(re.to, QString("S99E99"));

	QCOMPARE(failure([&] { RssJson::applyRegexp(re, QJsonObject{{"from", "0001"}}, feeds, queues, false); }), 422);
	QCOMPARE(failure([&] { RssJson::applyRegexp(re, QJsonObject{{"to", "S02E05|repack"}}, feeds, queues, false); }), 422);
	QCOMPARE(failure([&] { RssJson::applyRegexp(re, QJsonObject{{"from", "S01E01"}, {"to", "S02E05"}}, feeds, queues, false); }), 0);
	QCOMPARE(re.to, QString("S02E05"));

	RssJson::applyRegexp(re, QJsonObject{{"tvs", "date"}}, feeds, queues, false);
	QCOMPARE(re.from, QString("0000-00-00"));
	QCOMPARE(re.to, QString("9999-99-99"));

	RssJson::applyRegexp(re, QJsonObject{{"tvs", "none"}}, feeds, queues, false);
	QVERIFY(re.from.isEmpty() && re.to.isEmpty());
}

void TestRssResources::regexpPatchIsPartial()
{
	const QList<RssFeed> feeds = oneFeed();
	RssRegexp re;
	RssJson::applyRegexp(re, QJsonObject{{"feedId", "f1"}, {"queue", "q1"}, {"expression", "a"}, {"target", "/x"}, {"addPaused", true}}, feeds, queues, true);
	RssJson::applyRegexp(re, QJsonObject{{"expression", "b"}}, feeds, queues, false);
	QCOMPARE(re.regexp.pattern(), QString("b"));
	QCOMPARE(re.target, QString("/x"));
	QVERIFY(re.addPaused);
	QCOMPARE(failure([&] { RssJson::applyRegexp(re, QJsonObject{{"target", "  "}}, feeds, queues, false); }), 422);
	QCOMPARE(re.target, QString("/x"));
}

void TestRssResources::testEndpointMatchesTheEngine()
{
	QJsonObject r = RssJson::testRegexp(QJsonObject{{"expression", "SHOW.*720p"}, {"text", "show S01E02 720p"}});
	QVERIFY(r["matches"].toBool());
	QVERIFY(!r.contains("episode"));	// no tracking mode

	r = RssJson::testRegexp(QJsonObject{{"expression", "nomatch"}, {"text", "show S01E02"}, {"tvs", "season"}});
	QVERIFY(!r["matches"].toBool());
	QCOMPARE(r["episode"].toString(), QString("S01E02"));

	QCOMPARE(failure([] { RssJson::testRegexp(QJsonObject{{"expression", "("}, {"text", "x"}}); }), 422);
	QCOMPARE(failure([] { RssJson::testRegexp(QJsonObject{{"expression", "x"}}); }), 422);
}

void TestRssResources::downloadedEpisodesAreValidated()
{
	QCOMPARE(RssJson::episodesFromJson(QJsonArray{"S01E01", "S01E01", "S01E02"}, RssRegexp::SeasonBased), (QStringList{"S01E01", "S01E02"}));
	QCOMPARE(failure([] { RssJson::episodesFromJson(QJsonArray{"0001"}, RssRegexp::SeasonBased); }), 422);
	QCOMPARE(failure([] { RssJson::episodesFromJson(QJsonArray{"S01E01"}, RssRegexp::None); }), 409);
	QCOMPARE(failure([] { RssJson::episodesFromJson(QJsonValue("S01E01"), RssRegexp::SeasonBased); }), 422);
}

QTEST_APPLESS_MAIN(TestRssResources)
#include "tst_rssresources.moc"
