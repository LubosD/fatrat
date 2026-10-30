#include <QtTest>
#include "engines/TorrentSeeding.h"

class TestTorrentSeeding : public QObject
{
	Q_OBJECT
private slots:
	void zeroLimitStopsAtOnce();
	void ratioOfDownloaded();
	void alreadyCompleteUsesSize();
};

void TestTorrentSeeding::zeroLimitStopsAtOnce()
{
	QVERIFY(TorrentSeeding::ratioReached(0, 100, 100, 0.0));
	QVERIFY(TorrentSeeding::ratioReached(0, 0, 100, 0.0));
}

void TestTorrentSeeding::ratioOfDownloaded()
{
	QVERIFY(!TorrentSeeding::ratioReached(50, 100, 100, 1.0));
	QVERIFY(TorrentSeeding::ratioReached(100, 100, 100, 1.0));
	QVERIFY(TorrentSeeding::ratioReached(250, 100, 100, 2.0));
}

void TestTorrentSeeding::alreadyCompleteUsesSize()
{
	// nothing downloaded in this session: no NaN, measured against the size
	QVERIFY(!TorrentSeeding::ratioReached(0, 0, 1000, 1.5));
	QVERIFY(!TorrentSeeding::ratioReached(1499, 0, 1000, 1.5));
	QVERIFY(TorrentSeeding::ratioReached(1500, 0, 1000, 1.5));
	QVERIFY(!TorrentSeeding::ratioReached(0, 0, 0, 1.0));
}

QTEST_APPLESS_MAIN(TestTorrentSeeding)
#include "tst_torrentseeding.moc"
