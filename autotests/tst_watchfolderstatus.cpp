#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include "watch/WatchFolderStatus.h"

using WatchFolderStatus::Entry;

class TestWatchFolderStatus : public QObject
{
	Q_OBJECT
private slots:
	void normalize()
	{
		QCOMPARE(WatchFolderStatus::normalize(""), QString());
		QCOMPARE(WatchFolderStatus::normalize("/srv//watch/"), QString("/srv/watch"));
		QCOMPARE(WatchFolderStatus::normalize("/srv/a/../watch"), QString("/srv/watch"));
	}

	void owner()
	{
		const QList<Entry> e{ { "q1", "/srv/watch" }, { "q2", "" }, { "q3", "/srv/other" } };
		QCOMPARE(WatchFolderStatus::owner(e, "/srv/watch/"), QString("q1"));
		QCOMPARE(WatchFolderStatus::owner(e, "/srv/watch", "q1"), QString());
		QCOMPARE(WatchFolderStatus::owner(e, "/srv/none"), QString());
		QCOMPARE(WatchFolderStatus::owner(e, ""), QString());
	}

	void statuses()
	{
		QTemporaryDir tmp;
		const QString ok = tmp.path();
		const QString missing = tmp.filePath("missing");
		const QList<Entry> e{ { "q1", ok }, { "q2", "" }, { "q3", missing }, { "q4", ok + "/" } };

		QCOMPARE(WatchFolderStatus::status(e, "q1"), QString("ok"));
		QCOMPARE(WatchFolderStatus::status(e, "q2"), QString("off"));
		QCOMPARE(WatchFolderStatus::status(e, "q3"), QString("missing"));
		QCOMPARE(WatchFolderStatus::status(e, "q4"), QString("duplicate"));
		QCOMPARE(WatchFolderStatus::status(e, "unknown"), QString("off"));
	}

	void notWritable()
	{
		QTemporaryDir tmp;
		const QString dir = tmp.filePath("ro");
		QDir().mkdir(dir);
		QFile::setPermissions(dir, QFile::ReadOwner | QFile::ExeOwner);
		if (QFileInfo(dir).isWritable())
		{
			QFile::setPermissions(dir, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
			QSKIP("running with privileges that ignore permissions");
		}

		QCOMPARE(WatchFolderStatus::status({ { "q1", dir } }, "q1"), QString("not_writable"));
		QFile::setPermissions(dir, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
	}

	void describe()
	{
		QVERIFY(WatchFolderStatus::describe("ok").isEmpty());
		QVERIFY(WatchFolderStatus::describe("off").isEmpty());
		QVERIFY(!WatchFolderStatus::describe("missing").isEmpty());
		QVERIFY(!WatchFolderStatus::describe("not_writable").isEmpty());
		QVERIFY(!WatchFolderStatus::describe("duplicate").isEmpty());
	}
};

QTEST_GUILESS_MAIN(TestWatchFolderStatus)
#include "tst_watchfolderstatus.moc"
