#include <QDir>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include "python/PluginManifest.h"

/* Manifest parsing: what FatRat believes about a plugin before it runs any
 * of it. A manifest it misreads is a plugin that silently does not appear,
 * so the failure cases matter as much as the happy one.
 */
class tst_pluginmanifest : public QObject
{
	Q_OBJECT
private slots:
	void readsAPluginManifest();
	void defaultsTheEntryModule();
	void rejectsAnUnsupportedApiVersion();
	void rejectsBrokenJson();
	void rejectsAMissingManifest();
	void readsTheExampleRepository();
	void repositoryEntriesDefaultTheirSource();
	void rejectsANamelessRepository();
private:
	QString writePlugin(const QString& name, const QByteArray& json);

	QTemporaryDir m_dir;
};

QString tst_pluginmanifest::writePlugin(const QString& name, const QByteArray& json)
{
	QDir root(m_dir.path());

	root.mkpath(name);

	QFile file(root.filePath(name + "/plugin.json"));

	file.open(QIODevice::WriteOnly);
	file.write(json);
	file.close();

	return root.filePath(name);
}

void tst_pluginmanifest::readsAPluginManifest()
{
	QString dir = writePlugin("good", R"({
		"schema": 1,
		"name": "good",
		"version": "2.1.0",
		"api": "1",
		"description": "does things",
		"entry": "main.py",
		"license": "GPL-3.0-only",
		"permissions": ["network"],
		"config": [{"type": "text", "key": "good/user", "title": "Username"}]
	})");

	PluginManifest manifest = PluginManifest::read(dir, "1");

	QVERIFY2(manifest.valid, qPrintable(manifest.error));
	QCOMPARE(manifest.name, QString("good"));
	QCOMPARE(manifest.version, QString("2.1.0"));
	QCOMPARE(manifest.entry, QString("main.py"));
	QCOMPARE(manifest.permissions, QStringList() << "network");
	QCOMPARE(manifest.config.size(), 1);
	QCOMPARE(manifest.config.at(0).toObject().value("key").toString(), QString("good/user"));
}

void tst_pluginmanifest::defaultsTheEntryModule()
{
	QString dir = writePlugin("terse", R"({"name": "terse"})");
	PluginManifest manifest = PluginManifest::read(dir, "1");

	QVERIFY2(manifest.valid, qPrintable(manifest.error));
	QCOMPARE(manifest.entry, QString("terse.py"));
	// No "api" means "written against the API I am being read by".
	QCOMPARE(manifest.apiVersion, QString("1"));
}

void tst_pluginmanifest::rejectsAnUnsupportedApiVersion()
{
	QString dir = writePlugin("future", R"({"name": "future", "api": "2"})");
	PluginManifest manifest = PluginManifest::read(dir, "1");

	QVERIFY(!manifest.valid);
	QVERIFY2(manifest.error.contains("2"), qPrintable(manifest.error));
}

void tst_pluginmanifest::rejectsBrokenJson()
{
	QString dir = writePlugin("broken", "{ this is not json");
	PluginManifest manifest = PluginManifest::read(dir, "1");

	QVERIFY(!manifest.valid);
	QVERIFY(!manifest.error.isEmpty());
}

void tst_pluginmanifest::rejectsAMissingManifest()
{
	QDir(m_dir.path()).mkpath("empty");

	PluginManifest manifest = PluginManifest::read(QDir(m_dir.path()).filePath("empty"), "1");

	QVERIFY(!manifest.valid);
	QVERIFY(manifest.error.contains("plugin.json"));
}

void tst_pluginmanifest::readsTheExampleRepository()
{
	RepositoryManifest manifest = RepositoryManifest::read(EXAMPLE_REPO_DIR);

	QVERIFY2(manifest.valid, qPrintable(manifest.error));
	QCOMPARE(manifest.name, QString("example-plugins"));
	QVERIFY(!manifest.plugins.isEmpty());
	QCOMPARE(manifest.plugins[0].name, QString("dummy"));
	QCOMPARE(manifest.plugins[0].source, QString("./dummy"));

	// And the plugin it points at really is installable.
	PluginManifest plugin = PluginManifest::read(QDir(EXAMPLE_REPO_DIR).filePath("dummy"), "1");

	QVERIFY2(plugin.valid, qPrintable(plugin.error));
	QCOMPARE(plugin.entry, QString("dummy.py"));
	QVERIFY(!plugin.config.isEmpty());
}

void tst_pluginmanifest::repositoryEntriesDefaultTheirSource()
{
	RepositoryManifest manifest = RepositoryManifest::parse(
		R"({"name": "r", "plugins": [{"name": "a"}, {"description": "no name"}]})");

	QVERIFY2(manifest.valid, qPrintable(manifest.error));
	// The nameless entry is dropped: there would be nothing to install.
	QCOMPARE(manifest.plugins.size(), 1);
	QCOMPARE(manifest.plugins[0].source, QString("./a"));
}

void tst_pluginmanifest::rejectsANamelessRepository()
{
	RepositoryManifest manifest = RepositoryManifest::parse(R"({"plugins": []})");

	QVERIFY(!manifest.valid);
}

QTEST_GUILESS_MAIN(tst_pluginmanifest)
#include "tst_pluginmanifest.moc"
