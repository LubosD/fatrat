#include <QtTest>
#include "engines/TorrentEdgePieces.h"

class TestTorrentEdgePieces : public QObject
{
	Q_OBJECT
private slots:
	void fileInsideOnePiece();
	void fileSpanningPieces();
	void fileStartingMidPiece();
	void emptyOrInvalid();
};

void TestTorrentEdgePieces::fileInsideOnePiece()
{
	QCOMPARE(TorrentEdgePieces::ofFile(0, 100, 1000), std::make_pair(0, 0));
	QCOMPARE(TorrentEdgePieces::ofFile(2500, 100, 1000), std::make_pair(2, 2));
}

void TestTorrentEdgePieces::fileSpanningPieces()
{
	QCOMPARE(TorrentEdgePieces::ofFile(0, 1000, 1000), std::make_pair(0, 0));
	QCOMPARE(TorrentEdgePieces::ofFile(0, 1001, 1000), std::make_pair(0, 1));
	QCOMPARE(TorrentEdgePieces::ofFile(0, 5000, 1000), std::make_pair(0, 4));
}

void TestTorrentEdgePieces::fileStartingMidPiece()
{
	QCOMPARE(TorrentEdgePieces::ofFile(1500, 1500, 1000), std::make_pair(1, 2));
	QCOMPARE(TorrentEdgePieces::ofFile(1500, 500, 1000), std::make_pair(1, 1));
}

void TestTorrentEdgePieces::emptyOrInvalid()
{
	const std::pair<int,int> none(-1, -1);
	QCOMPARE(TorrentEdgePieces::ofFile(0, 0, 1000), none);
	QCOMPARE(TorrentEdgePieces::ofFile(0, 10, 0), none);
	QCOMPARE(TorrentEdgePieces::ofFile(-1, 10, 1000), none);
}

QTEST_APPLESS_MAIN(TestTorrentEdgePieces)
#include "tst_torrentedgepieces.moc"
