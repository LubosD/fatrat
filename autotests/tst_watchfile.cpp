#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include "watch/WatchFile.h"

class TestWatchFile : public QObject
{
	Q_OBJECT
	QTemporaryDir m_dir;

	QString write(const QString& name, const QByteArray& data)
	{
		const QString path = m_dir.filePath(name);
		QFile f(path);
		if (!f.open(QIODevice::WriteOnly))
			qFatal("cannot write %s", qPrintable(path));
		f.write(data);
		return path;
	}
private slots:
	void kinds_data()
	{
		QTest::addColumn<QString>("name");
		QTest::addColumn<int>("kind");
		QTest::newRow("torrent") << "a.torrent" << int(WatchFile::Torrent);
		QTest::newRow("upper") << "A.TORRENT" << int(WatchFile::Torrent);
		QTest::newRow("metalink") << "a.metalink" << int(WatchFile::Metalink);
		QTest::newRow("meta4") << "a.meta4" << int(WatchFile::Metalink);
		QTest::newRow("txt") << "links.txt" << int(WatchFile::LinkList);
		QTest::newRow("magnet") << "x.magnet" << int(WatchFile::LinkList);
		QTest::newRow("hidden") << ".a.torrent" << int(WatchFile::Unsupported);
		QTest::newRow("partial") << "a.torrent.part" << int(WatchFile::Unsupported);
		QTest::newRow("added") << "a.torrent.added" << int(WatchFile::Unsupported);
		QTest::newRow("failed") << "a.txt.failed" << int(WatchFile::Unsupported);
		QTest::newRow("other") << "a.zip" << int(WatchFile::Unsupported);
	}
	void kinds()
	{
		QFETCH(QString, name);
		QFETCH(int, kind);
		QCOMPARE(int(WatchFile::kind(name)), kind);
	}

	void torrentIsItsOwnSource()
	{
		const QString path = write("t.torrent", "d4:infoe");
		const WatchFile::Contents c = WatchFile::read(path);
		QVERIFY(c.error.isEmpty());
		QCOMPARE(c.sources, QStringList{ QFileInfo(path).absoluteFilePath() });
	}

	void linkListSkipsBlankAndComments()
	{
		const QString path = write("l.txt", "\xEF\xBB\xBFhttp://a/1\r\n\n  # comment\n  http://b/2  \n");
		const WatchFile::Contents c = WatchFile::read(path);
		QVERIFY(c.error.isEmpty());
		QCOMPARE(c.sources, (QStringList{ "http://a/1", "http://b/2" }));
	}

	void emptyLinkListIsAnError()
	{
		const WatchFile::Contents c = WatchFile::read(write("e.magnet", "# nothing\n"));
		QVERIFY(c.sources.isEmpty());
		QVERIFY(!c.error.isEmpty());
	}

	void oversizedLinkListIsAnError()
	{
		const WatchFile::Contents c = WatchFile::read(write("big.txt", QByteArray(WatchFile::MaxListSize + 1, 'a')));
		QVERIFY(c.sources.isEmpty());
		QVERIFY(!c.error.isEmpty());
	}

	void linkListAtTheLimitIsRead()
	{
		QByteArray data = "http://a/\n";
		data.append(QByteArray(WatchFile::MaxListSize - data.size(), '\n'));
		const WatchFile::Contents c = WatchFile::read(write("limit.txt", data));
		QVERIFY(c.error.isEmpty());
		QCOMPARE(c.sources, QStringList{ "http://a/" });
	}

	void unsupportedFileIsAnError()
	{
		const WatchFile::Contents c = WatchFile::read(write("notes.zip", "x"));
		QVERIFY(c.sources.isEmpty());
		QVERIFY(!c.error.isEmpty());
	}

	void markProcessedRenames()
	{
		const QString path = write("m.torrent", "x");
		QCOMPARE(WatchFile::markProcessed(path, true), path + ".added");
		QVERIFY(!QFile::exists(path));
		QVERIFY(QFile::exists(path + ".added"));

		const QString failed = write("f.txt", "x");
		QCOMPARE(WatchFile::markProcessed(failed, false), failed + ".failed");
	}

	void markProcessedAvoidsCollisions()
	{
		const QString path = write("c.torrent", "1");
		write("c.torrent.added", "old");
		write("c.torrent.added.1", "older");
		QCOMPARE(WatchFile::markProcessed(path, true), path + ".added.2");

		QFile old(path + ".added"), older(path + ".added.1");
		QVERIFY(old.open(QIODevice::ReadOnly) && older.open(QIODevice::ReadOnly));
		QCOMPARE(old.readAll(), QByteArray("old"));
		QCOMPARE(older.readAll(), QByteArray("older"));
	}

	void markProcessedReportsFailure()
	{
		QCOMPARE(WatchFile::markProcessed(m_dir.filePath("missing.torrent"), true), QString());
	}
};

QTEST_GUILESS_MAIN(TestWatchFile)
#include "tst_watchfile.moc"
