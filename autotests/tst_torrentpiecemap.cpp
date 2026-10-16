#include <QtTest>
#include "engines/TorrentPieceMap.h"

class TestTorrentPieceMap : public QObject
{
	Q_OBJECT
private slots:
	void completePiecesRenderBlue();
	void missingPiecesRenderWhite();
	void zeroAvailabilityRendersWhite();
	void fullAvailabilityRendersBlack();
};

void TestTorrentPieceMap::completePiecesRenderBlue()
{
	libtorrent::bitfield pieces(4, true);
	quint32 buf[4];
	TorrentPieceMap::renderPieces(pieces, 4, buf);
	for (quint32 px : buf)
		QCOMPARE(px, 0xff0000ffu);
}

void TestTorrentPieceMap::missingPiecesRenderWhite()
{
	libtorrent::bitfield pieces(4, false);
	quint32 buf[4];
	TorrentPieceMap::renderPieces(pieces, 4, buf);
	for (quint32 px : buf)
		QCOMPARE(px, 0xffffffffu);
}

void TestTorrentPieceMap::zeroAvailabilityRendersWhite()
{
	std::vector<int> avail(4, 0);
	quint32 buf[4];
	TorrentPieceMap::renderAvailability(avail, 4, buf);
	for (quint32 px : buf)
		QCOMPARE(px, 0xffffffffu);
}

void TestTorrentPieceMap::fullAvailabilityRendersBlack()
{
	std::vector<int> avail(4, 3);
	quint32 buf[4];
	TorrentPieceMap::renderAvailability(avail, 4, buf);
	for (quint32 px : buf)
		QCOMPARE(px, 0xff000000u);
}

QTEST_APPLESS_MAIN(TestTorrentPieceMap)
#include "tst_torrentpiecemap.moc"
