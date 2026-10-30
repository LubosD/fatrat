#include <QtTest>
#include <QSettings>
#include <QTemporaryDir>
#include <QPoint>
#include "util/SettingsString.h"

class tst_SettingsString : public QObject
{
	Q_OBJECT
private slots:
	void commaPasswordFromHandEditedFile()
	{
		QTemporaryDir dir;
		const QString path = dir.filePath("fatrat.conf");
		QFile f(path);
		QVERIFY(f.open(QIODevice::WriteOnly));
		f.write("[remote]\npassword=a,b\nplain=secret\nquoted=\"x,y\"\nlist=a, b , c\nempty=\ntrailing=a,\n");
		f.close();

		QSettings s(path, QSettings::IniFormat);
		bool unreadable = true;

		// QSettings itself reads this as a list, whose toString() is empty: that used to turn auth off
		QVERIFY(s.value("remote/password").toString().isEmpty());
		QCOMPARE(settingToString(s.value("remote/password"), &unreadable), QString("a,b"));
		QVERIFY(!unreadable);

		QCOMPARE(settingToString(s.value("remote/plain")), QString("secret"));
		QCOMPARE(settingToString(s.value("remote/quoted")), QString("x,y"));
		QCOMPARE(settingToString(s.value("remote/trailing")), QString("a,"));
		QCOMPARE(settingToString(s.value("remote/empty"), &unreadable), QString());
		QVERIFY(!unreadable);
		QCOMPARE(settingToString(s.value("remote/missing"), &unreadable), QString());
		QVERIFY(!unreadable);
	}

	void roundTripThroughSetValue()
	{
		QTemporaryDir dir;
		const QString path = dir.filePath("fatrat.conf");
		{
			QSettings s(path, QSettings::IniFormat);
			s.setValue("remote/password", "p,w");
		}
		QSettings s(path, QSettings::IniFormat);
		QCOMPARE(settingToString(s.value("remote/password")), QString("p,w"));
	}

	void unreadableValuesAreFlagged()
	{
		bool unreadable = false;
		QCOMPARE(settingToString(QVariant::fromValue(QPoint(1, 2)), &unreadable), QString());
		QVERIFY(unreadable);
		unreadable = true;
		QCOMPARE(settingToString(QVariant(QString()), &unreadable), QString());
		QVERIFY(!unreadable);
	}
};

QTEST_GUILESS_MAIN(tst_SettingsString)
#include "tst_settingsstring.moc"
