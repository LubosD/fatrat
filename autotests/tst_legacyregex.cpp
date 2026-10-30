#include <QtTest>
#include <QSettings>
#include "util/LegacyRegex.h"

// Pins the QRegExp semantics that legacyRegex() preserves for patterns users
// have saved in settings and plugins.
class TestLegacyRegex : public QObject
{
	Q_OBJECT
private slots:
	void dotMatchesNewline();
	void wordCharactersAreUnicodeAware();
	void digitsAreUnicodeAware();
	void caseInsensitiveOption();
	void anchoredPatternRequiresWholeString();
	void anchoredPatternKeepsAlternationTogether();
	void anchoredDotStarSpansNewlines();
	void invalidPatternIsReported();
	void globalMatchFindsEveryLink();
	void clipboardDefaultsMatchHttps();
};

void TestLegacyRegex::dotMatchesNewline()
{
	QVERIFY(legacyRegex("x.*y").match("x\ny").hasMatch());
}

void TestLegacyRegex::wordCharactersAreUnicodeAware()
{
	QVERIFY(legacyRegex("^\\w+$").match(QString::fromUtf8("\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82")).hasMatch());
}

void TestLegacyRegex::digitsAreUnicodeAware()
{
	QVERIFY(legacyRegex("^\\d+$").match(QString::fromUtf8("\xd9\xa3\xd9\xa4\xd9\xa5")).hasMatch());
}

void TestLegacyRegex::caseInsensitiveOption()
{
	QVERIFY(!legacyRegex("abc").match("ABC").hasMatch());
	QVERIFY(legacyRegex("abc", QRegularExpression::CaseInsensitiveOption).match("ABC").hasMatch());
}

void TestLegacyRegex::anchoredPatternRequiresWholeString()
{
	QRegularExpression re = legacyRegex(QRegularExpression::anchoredPattern("a+"));
	QVERIFY(re.match("aaa").hasMatch());
	QVERIFY(!re.match("baaa").hasMatch());
	QVERIFY(!re.match("aaab").hasMatch());
}

void TestLegacyRegex::anchoredPatternKeepsAlternationTogether()
{
	QRegularExpression re = legacyRegex(QRegularExpression::anchoredPattern("a|b"));
	QVERIFY(re.match("a").hasMatch());
	QVERIFY(re.match("b").hasMatch());
	QVERIFY(!re.match("ab").hasMatch());
}

void TestLegacyRegex::anchoredDotStarSpansNewlines()
{
	QRegularExpression re = legacyRegex(QRegularExpression::anchoredPattern("\"([^\"]+)\".*"));
	QRegularExpressionMatch m = re.match("\"quoted name\"; x=1\r\n");
	QVERIFY(m.hasMatch());
	QCOMPARE(m.captured(1), QString("quoted name"));
}

void TestLegacyRegex::invalidPatternIsReported()
{
	QVERIFY(!legacyRegex("(").isValid());
	QVERIFY(legacyRegex("").isValid());
}

void TestLegacyRegex::globalMatchFindsEveryLink()
{
	QRegularExpressionMatchIterator it = legacyRegex("https?://[^\\s]+").globalMatch("https://a.b/c.torrent and http://d.e/f");
	QStringList links;
	while (it.hasNext())
		links << it.next().captured(0);
	QCOMPARE(links, QStringList() << "https://a.b/c.torrent" << "http://d.e/f");
}

void TestLegacyRegex::clipboardDefaultsMatchHttps()
{
	QSettings defaults(DEFAULTS_CONF, QSettings::IniFormat);
	const QStringList patterns = defaults.value("clipboard/regexps").toStringList();
	QCOMPARE(patterns.size(), 3);
	for (const QString& p : patterns)
	{
		const QString ext = p.mid(p.lastIndexOf('.') + 1);
		for (const char* scheme : { "http", "https", "ftp" })
		{
			const QString url = QString("%1://example.com/a.%2").arg(scheme, ext);
			QCOMPARE(legacyRegex(p).match(url).captured(0), url);
		}
		QVERIFY(!legacyRegex(p).match("gopher://example.com/a." + ext).hasMatch());
	}
}

QTEST_APPLESS_MAIN(TestLegacyRegex)
#include "tst_legacyregex.moc"
