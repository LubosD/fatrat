#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include "watch/WatchFolderService.h"
#include "RuntimeException.h"

struct AddCall
{
	QString queue;
	QStringList sources;
};

class TestWatchFolderService : public QObject
{
	Q_OBJECT
	QTemporaryDir* m_tmp = nullptr;
	qint64 m_now = 0;
	QList<WatchFolderStatus::Entry> m_entries;
	QList<AddCall> m_calls;
	bool m_throw = false;
	bool m_lockDirOnAdd = false;

	QString dir() const { return m_tmp->path(); }
	void write(const QString& name, const QByteArray& data)
	{
		QFile f(QDir(dir()).filePath(name));
		if (!f.open(QIODevice::WriteOnly))
			qFatal("cannot write");
		f.write(data);
	}
	bool exists(const QString& name) const { return QFile::exists(QDir(dir()).filePath(name)); }

	// Sources containing "bad" are rejected, everything else becomes one transfer.
	WatchFolderService::Hooks hooks()
	{
		WatchFolderService::Hooks h;
		h.folders = [this] { return m_entries; };
		h.add = [this](const QString& queue, const QStringList& sources, QStringList& rejected) -> int {
			if (m_throw)
				throw RuntimeException("boom");
			m_calls << AddCall{ queue, sources };
			if (m_lockDirOnAdd)
				QFile::setPermissions(dir(), QFile::ReadOwner | QFile::ExeOwner);
			int n = 0;
			for (const QString& s : sources)
			{
				if (s.contains("bad"))
					rejected << s;
				else
					n++;
			}
			return n;
		};
		h.clock = [this] { return m_now; };
		return h;
	}
	void settle(WatchFolderService& s, const QString& d)
	{
		s.scan(d);
		m_now += WatchFolderScanner::SettleMs;
		s.scan(d);
	}
private slots:
	void init()
	{
		m_tmp = new QTemporaryDir;
		m_now = 0;
		m_calls.clear();
		m_throw = false;
		m_lockDirOnAdd = false;
		m_entries = { { "q1", dir() } };
	}
	void cleanup()
	{
		// a failed QVERIFY must not leave a directory that cannot be deleted
		QFile::setPermissions(dir(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
		delete m_tmp;
	}

	void addsTorrentAndRenames()
	{
		WatchFolderService s(hooks());
		write("a.torrent", "x");

		s.scan(dir());
		QVERIFY(m_calls.isEmpty());

		m_now += WatchFolderScanner::SettleMs;
		s.scan(dir());
		QCOMPARE(m_calls.size(), 1);
		QCOMPARE(m_calls[0].queue, QString("q1"));
		QCOMPARE(m_calls[0].sources, QStringList{ QDir(dir()).filePath("a.torrent") });
		QVERIFY(exists("a.torrent.added"));
		QVERIFY(!exists("a.torrent"));
	}

	void linkListWithSomeBadLinksIsAdded()
	{
		WatchFolderService s(hooks());
		write("l.txt", "http://good/1\nhttp://bad/2\n");
		settle(s, dir());
		QCOMPARE(m_calls.size(), 1);
		QCOMPARE(m_calls[0].sources, (QStringList{ "http://good/1", "http://bad/2" }));
		QVERIFY(exists("l.txt.added"));
	}

	void nothingAddedIsFailed()
	{
		WatchFolderService s(hooks());
		write("l.magnet", "magnet:?bad\n");
		settle(s, dir());
		QVERIFY(exists("l.magnet.failed"));
	}

	void unreadableListIsFailedWithoutAdding()
	{
		WatchFolderService s(hooks());
		write("empty.txt", "\n# nothing\n");
		settle(s, dir());
		QVERIFY(m_calls.isEmpty());
		QVERIFY(exists("empty.txt.failed"));
	}

	void hookErrorIsFailed()
	{
		WatchFolderService s(hooks());
		m_throw = true;
		write("a.torrent", "x");
		settle(s, dir());
		QVERIFY(exists("a.torrent.failed"));
	}

	void collisionGetsSuffix()
	{
		WatchFolderService s(hooks());
		write("a.torrent.added", "old");
		write("a.torrent", "new");
		settle(s, dir());
		QVERIFY(exists("a.torrent.added.1"));
	}

	void duplicateFolderGoesToFirstQueue()
	{
		m_entries = { { "q1", dir() }, { "q2", dir() + "/" } };
		WatchFolderService s(hooks());
		write("a.torrent", "x");
		settle(s, dir());
		QCOMPARE(m_calls.size(), 1);
		QCOMPARE(m_calls[0].queue, QString("q1"));
	}

	void notWritableFolderIsSkipped()
	{
		write("a.torrent", "x");
		QFile::setPermissions(dir(), QFile::ReadOwner | QFile::ExeOwner);
		if (QFileInfo(dir()).isWritable())
			QSKIP("running with privileges that ignore permissions");

		WatchFolderService s(hooks());
		settle(s, dir());
		QVERIFY(m_calls.isEmpty());
		QVERIFY(exists("a.torrent"));
	}

	void missingFolderIsPickedUpOnceCreated()
	{
		const QString sub = QDir(dir()).filePath("later");
		m_entries = { { "q1", sub } };
		WatchFolderService s(hooks());

		QDir().mkdir(sub);
		{
			QFile f(QDir(sub).filePath("a.torrent"));
			QVERIFY(f.open(QIODevice::WriteOnly));
		}
		s.reload();
		settle(s, sub);
		QCOMPARE(m_calls.size(), 1);
		QVERIFY(QFile::exists(QDir(sub).filePath("a.torrent.added")));
	}

	void renameFailureIsNotAddedAgain()
	{
		QFile::setPermissions(dir(), QFile::ReadOwner | QFile::ExeOwner);
		const bool locks = !QFileInfo(dir()).isWritable();
		QFile::setPermissions(dir(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
		if (!locks)
			QSKIP("running with privileges that ignore permissions");

		WatchFolderService s(hooks());
		m_lockDirOnAdd = true;
		write("a.torrent", "x");
		settle(s, dir());
		QCOMPARE(m_calls.size(), 1);
		QVERIFY(exists("a.torrent"));

		for (int i = 0; i < 2; i++)
		{
			m_now += WatchFolderScanner::SettleMs;
			s.scan(dir());
			m_now += WatchFolderScanner::SettleMs;
			s.scan(dir());
		}
		QCOMPARE(m_calls.size(), 1);

		m_lockDirOnAdd = false;
		QFile::setPermissions(dir(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
		s.reload(); // the locked folder was dropped as not writable
		write("a.torrent", "different content");
		settle(s, dir());
		QCOMPARE(m_calls.size(), 2);
		QVERIFY(exists("a.torrent.added"));
	}

	void realEventsAddDroppedFile()
	{
		WatchFolderService::Hooks h = hooks();
		h.clock = nullptr;
		WatchFolderService s(h);
		write("a.torrent", "x");
		QTRY_VERIFY_WITH_TIMEOUT(exists("a.torrent.added"), 10000);
		QCOMPARE(m_calls.size(), 1);
	}

	void deletedQueueLeavesFilesAlone()
	{
		WatchFolderService s(hooks());
		write("a.torrent", "x");
		s.scan(dir());

		m_entries = {}; // no reload(): the service does not know yet
		m_now += WatchFolderScanner::SettleMs;
		s.scan(dir());
		QVERIFY(m_calls.isEmpty());
		QVERIFY(exists("a.torrent"));
	}

	void unwatchedFolderIsIgnored()
	{
		WatchFolderService s(hooks());
		m_entries = { { "q1", "" } };
		s.reload();
		write("a.torrent", "x");
		settle(s, dir());
		QVERIFY(m_calls.isEmpty());
	}
};

QTEST_GUILESS_MAIN(TestWatchFolderService)
#include "tst_watchfolderservice.moc"
