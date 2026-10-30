#include <QtTest>
#include "engines/TorrentSessionOptions.h"

class TestTorrentSessionOptions : public QObject
{
	Q_OBJECT
private slots:
	void listenRange();
	void listenRangeDegenerate();
	void announceIp();
};

void TestTorrentSessionOptions::listenRange()
{
	QCOMPARE(TorrentSessionOptions::listenInterfaces(6881), QString("0.0.0.0:6881, [::]:6881"));
	QCOMPARE(TorrentSessionOptions::listenRetries(6881, 6888), 7);
	QCOMPARE(TorrentSessionOptions::listenRetries(6881, 6881), 0);
}

void TestTorrentSessionOptions::listenRangeDegenerate()
{
	QCOMPARE(TorrentSessionOptions::listenRetries(6888, 6881), 0);
	QCOMPARE(TorrentSessionOptions::listenRetries(65530, 70000), 5);
}

void TestTorrentSessionOptions::announceIp()
{
	QCOMPARE(TorrentSessionOptions::announceIp(" 203.0.113.7 "), QString("203.0.113.7"));
	QCOMPARE(TorrentSessionOptions::announceIp("2001:db8::1"), QString("2001:db8::1"));
	QCOMPARE(TorrentSessionOptions::announceIp(""), QString());
	QCOMPARE(TorrentSessionOptions::announceIp("example.com"), QString());
}

QTEST_APPLESS_MAIN(TestTorrentSessionOptions)
#include "tst_torrentsessionoptions.moc"
