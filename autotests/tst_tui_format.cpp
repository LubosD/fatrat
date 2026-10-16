#include "Format.h"
#include <QtTest>

using namespace tui;

class TestTuiFormat : public QObject
{
	Q_OBJECT
private slots:
	void sizes()
	{
		QCOMPARE(formatSize(0), QString("0 B"));
		QCOMPARE(formatSize(-5), QString("0 B"));
		QCOMPARE(formatSize(512), QString("512 B"));
		QCOMPARE(formatSize(1536), QString("1.5 KB"));
		QCOMPARE(formatSize(qint64(4.2 * 1024 * 1024 * 1024)), QString("4.2 GB"));
		QCOMPARE(formatSpeed(1024), QString("1.0 KB/s"));
	}
	void eta()
	{
		const QString dash = QString::fromUtf8("—");
		QCOMPARE(formatEta(100, 100, 10), dash);
		QCOMPARE(formatEta(100, 0, 0), dash);
		QCOMPARE(formatEta(100, 50, 10), QString("5s"));
		QCOMPARE(formatEta(7300, 0, 10), QString("12m 10s"));
		QCOMPARE(formatEta(36000, 0, 10), QString("1h 0m"));
	}
	void widths()
	{
		QCOMPARE(displayWidth("abc"), 3);
		QCOMPARE(displayWidth(QString::fromUtf8("日本")), 4);
		QCOMPARE(fit("abcdef", 4), QString("abcd"));
		QCOMPARE(fit("ab", 4), QString("ab  "));
		QCOMPARE(fitRight("ab", 4), QString("  ab"));
		QCOMPARE(elide("abcdefgh", 5), QString::fromUtf8("abcd…"));
		QCOMPARE(elide("abc", 5), QString("abc"));
		QCOMPARE(displayWidth(QString::fromUtf8("e\xcc\x81")), 1);          // e + combining acute
		QCOMPARE(displayWidth(QString::fromUtf8("\xf0\x9f\x9a\x80")), 2); // rocket
		// a wide character must not be cut in half
		QCOMPARE(displayWidth(fit(QString::fromUtf8("日本語"), 5)), 5);
	}
};

QTEST_APPLESS_MAIN(TestTuiFormat)
#include "tst_tui_format.moc"
