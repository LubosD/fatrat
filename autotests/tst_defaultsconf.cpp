#include <QtTest>
#include <QSettings>

class TestDefaultsConf : public QObject
{
	Q_OBJECT
private slots:
	void pythonUpdateCheckDefault();
	void listenRange();
};

static QSettings defaults()
{
	return QSettings(QString(SOURCE_DIR "/data/defaults.conf"), QSettings::IniFormat);
}

// MainWindow and PluginUpdateChecker read python/check_updates
void TestDefaultsConf::pythonUpdateCheckDefault()
{
	QSettings s = defaults();
	QVERIFY(s.contains("python/check_updates"));
	QVERIFY(s.value("python/check_updates").toBool());
	QVERIFY(!s.contains("java/check_updates"));
}

void TestDefaultsConf::listenRange()
{
	QSettings s = defaults();
	QVERIFY(s.value("torrent/listen_start").toInt() <= s.value("torrent/listen_end").toInt());
}

QTEST_APPLESS_MAIN(TestDefaultsConf)
#include "tst_defaultsconf.moc"
