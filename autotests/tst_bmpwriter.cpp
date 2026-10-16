#include <QtTest>
#include <QtEndian>
#include "util/BmpWriter.h"

class TestBmpWriter : public QObject
{
	Q_OBJECT
private slots:
	void headerDescribesTopDown32bppImage();
	void pixelsAreStoredAsBgra();
};

static quint32 le32(const QByteArray& b, int offset)
{
	return qFromLittleEndian<quint32>(b.constData() + offset);
}

static quint16 le16(const QByteArray& b, int offset)
{
	return qFromLittleEndian<quint16>(b.constData() + offset);
}

void TestBmpWriter::headerDescribesTopDown32bppImage()
{
	const quint32 pixels[3] = { 0xffff0000, 0xff00ff00, 0xff0000ff };
	QByteArray bmp = encodeBmp(pixels, 3, 1);

	QCOMPARE(bmp.size(), 14 + 40 + 3*4);
	QCOMPARE(bmp.left(2), QByteArray("BM"));
	QCOMPARE(le32(bmp, 2), quint32(bmp.size()));
	QCOMPARE(le32(bmp, 10), 54u);
	QCOMPARE(le32(bmp, 14), 40u);
	QCOMPARE(qint32(le32(bmp, 18)), 3);
	QCOMPARE(qint32(le32(bmp, 22)), -1);
	QCOMPARE(le16(bmp, 26), quint16(1));
	QCOMPARE(le16(bmp, 28), quint16(32));
	QCOMPARE(le32(bmp, 30), 0u);
	QCOMPARE(le32(bmp, 34), 12u);
}

void TestBmpWriter::pixelsAreStoredAsBgra()
{
	const quint32 pixels[2] = { 0xff112233, 0xff445566 };
	QByteArray bmp = encodeBmp(pixels, 2, 1);

	QCOMPARE(bmp.mid(54), QByteArray("\x33\x22\x11\xff\x66\x55\x44\xff", 8));
}

QTEST_APPLESS_MAIN(TestBmpWriter)
#include "tst_bmpwriter.moc"
