#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include "watch/WatchFolderScanner.h"

class TestWatchFolderScanner : public QObject
{
	Q_OBJECT
	QTemporaryDir* m_dir = nullptr;
	qint64 m_now = 0;

	void write(const QString& name, const QByteArray& data, QIODevice::OpenMode mode = QIODevice::WriteOnly)
	{
		QFile f(m_dir->filePath(name));
		if (!f.open(mode))
			qFatal("cannot write");
		f.write(data);
	}
	WatchFolderScanner scanner() { return WatchFolderScanner([this] { return m_now; }); }
private slots:
	void init() { m_dir = new QTemporaryDir; m_now = 1000; }
	void cleanup() { delete m_dir; }

	void readyOnlyAfterSettling()
	{
		WatchFolderScanner s = scanner();
		write("a.torrent", "x");

		WatchFolderScanner::Result r = s.scan(m_dir->path());
		QVERIFY(r.ready.isEmpty());
		QVERIFY(r.pending);

		m_now += WatchFolderScanner::SettleMs - 1;
		QVERIFY(s.scan(m_dir->path()).ready.isEmpty());

		m_now += 1;
		r = s.scan(m_dir->path());
		QCOMPARE(r.ready, QStringList{ m_dir->filePath("a.torrent") });
		QVERIFY(!r.pending);
	}

	void growingFileStartsOver()
	{
		WatchFolderScanner s = scanner();
		write("b.txt", "http://a/\n");
		s.scan(m_dir->path());

		m_now += WatchFolderScanner::SettleMs;
		write("b.txt", "http://b/\n", QIODevice::Append);
		WatchFolderScanner::Result r = s.scan(m_dir->path());
		QVERIFY(r.ready.isEmpty());
		QVERIFY(r.pending);

		m_now += WatchFolderScanner::SettleMs;
		QCOMPARE(s.scan(m_dir->path()).ready.size(), 1);
	}

	// The service renames processed files away; a file that comes back under the same name settles again.
	void reappearingFileSettlesAgain()
	{
		WatchFolderScanner s = scanner();
		write("c.torrent", "x");
		s.scan(m_dir->path());
		m_now += WatchFolderScanner::SettleMs;
		QCOMPARE(s.scan(m_dir->path()).ready.size(), 1);

		QFile::remove(m_dir->filePath("c.torrent"));
		QVERIFY(s.scan(m_dir->path()).ready.isEmpty());

		write("c.torrent", "x");
		WatchFolderScanner::Result r = s.scan(m_dir->path());
		QVERIFY(r.ready.isEmpty());
		QVERIFY(r.pending);
	}

	void ignoresOtherFiles()
	{
		WatchFolderScanner s = scanner();
		write(".hidden.torrent", "x");
		write("a.torrent.part", "x");
		write("a.torrent.added", "x");
		write("notes.doc", "x");
		QDir(m_dir->path()).mkdir("sub.torrent");

		s.scan(m_dir->path());
		m_now += WatchFolderScanner::SettleMs;
		WatchFolderScanner::Result r = s.scan(m_dir->path());
		QVERIFY(r.ready.isEmpty());
		QVERIFY(!r.pending);
	}

	void missingFolderIsEmpty()
	{
		WatchFolderScanner s = scanner();
		WatchFolderScanner::Result r = s.scan(m_dir->filePath("nope"));
		QVERIFY(r.ready.isEmpty());
		QVERIFY(!r.pending);
	}
};

QTEST_GUILESS_MAIN(TestWatchFolderScanner)
#include "tst_watchfolderscanner.moc"
