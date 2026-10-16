#include <QDir>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

#include "python/PluginRepository.h"

/* Adding repositories and installing from them, against a git repository made
 * here in a temp directory -- no network, and no FatRat.
 */
class tst_pluginrepository : public QObject
{
	Q_OBJECT
private slots:
	void init();

	void addsALocalDirectoryRepository();
	void rejectsADirectoryThatIsNotARepository();
	void refusesTheSameRepositoryTwice();
	void installsFromALocalRepositoryBySymlink();
	void uninstallForgetsThePlugin();
	void removingARepositoryLeavesInstalledPluginsAlone();
	void clonesAGitRepositoryAndPinsItsRevision();
	void reportsAnUpdateAfterTheRepositoryMovesOn();
private:
	QString makeRepository(const QString& name, const QString& version);
	bool git(const QStringList& arguments, const QString& directory);
	// Commits everything in `directory`, with an identity of its own and no
	// signing -- the machine running the tests may well sign by default.
	bool commitAll(const QString& directory, const QString& message);

	QTemporaryDir m_dir;
	QString m_root;
};

void tst_pluginrepository::init()
{
	// Each test gets its own plugins root, so none of them sees another's
	// repos.json.
	static int counter = 0;

	m_root = QDir(m_dir.path()).filePath(QString("root%1").arg(++counter));
	QDir().mkpath(m_root);

	PluginRepository::setRoot(m_root);
}

bool tst_pluginrepository::git(const QStringList& arguments, const QString& directory)
{
	QProcess process;

	process.setWorkingDirectory(directory);
	process.start("git", arguments);
	process.waitForFinished(30000);

	return process.exitCode() == 0;
}

bool tst_pluginrepository::commitAll(const QString& directory, const QString& message)
{
	if (!git(QStringList() << "add" << "-A", directory))
		return false;

	return git(QStringList() << "-c" << "user.email=tests@example.com"
				 << "-c" << "user.name=FatRat tests"
				 << "-c" << "commit.gpgsign=false"
				 << "commit" << "-q" << "-m" << message, directory);
}

QString tst_pluginrepository::makeRepository(const QString& name, const QString& version)
{
	QString path = QDir(m_dir.path()).filePath(name);

	QDir().mkpath(path + "/sample");

	QFile repository(path + "/fatrat-plugins.json");

	repository.open(QIODevice::WriteOnly);
	repository.write(QString(R"({"schema": 1, "name": "%1", "description": "test repo",
		"plugins": [{"name": "sample", "source": "./sample", "description": "a sample"}]})")
		.arg(name).toUtf8());
	repository.close();

	QFile plugin(path + "/sample/plugin.json");

	plugin.open(QIODevice::WriteOnly);
	plugin.write(QString(R"({"schema": 1, "name": "sample", "version": "%1", "api": "1",
		"entry": "sample.py"})").arg(version).toUtf8());
	plugin.close();

	QFile module(path + "/sample/sample.py");

	module.open(QIODevice::WriteOnly);
	module.write("# a sample plugin\n");
	module.close();

	return path;
}

void tst_pluginrepository::addsALocalDirectoryRepository()
{
	QString path = makeRepository("local-repo", "1.0.0");
	QString error;

	QVERIFY2(PluginRepository::addRepository(path, &error), qPrintable(error));

	QList<PluginRepository::Repository> repositories = PluginRepository::repositories();

	QCOMPARE(repositories.size(), 1);
	QCOMPARE(repositories[0].name, QString("local-repo"));
	QVERIFY(repositories[0].local);

	QList<PluginRepository::Available> available = PluginRepository::available();

	QCOMPARE(available.size(), 1);
	QCOMPARE(available[0].plugin.name, QString("sample"));
	// The plugin's own manifest wins over the repository's listing.
	QCOMPARE(available[0].plugin.version, QString("1.0.0"));
	QVERIFY(available[0].installedVersion.isEmpty());
}

void tst_pluginrepository::rejectsADirectoryThatIsNotARepository()
{
	QString path = QDir(m_dir.path()).filePath("not-a-repo");
	QString error;

	QDir().mkpath(path);

	QVERIFY(!PluginRepository::addRepository(path, &error));
	QVERIFY(!error.isEmpty());
	QVERIFY(PluginRepository::repositories().isEmpty());
}

void tst_pluginrepository::refusesTheSameRepositoryTwice()
{
	QString path = makeRepository("twice", "1.0.0");
	QString error;

	QVERIFY(PluginRepository::addRepository(path, &error));
	QVERIFY(!PluginRepository::addRepository(path, &error));
	QCOMPARE(PluginRepository::repositories().size(), 1);
}

void tst_pluginrepository::installsFromALocalRepositoryBySymlink()
{
	QString path = makeRepository("install-from", "1.2.3");
	QString error;

	QVERIFY2(PluginRepository::addRepository(path, &error), qPrintable(error));
	QVERIFY2(PluginRepository::install("install-from", "sample", &error), qPrintable(error));

	QFileInfo installed(QDir(PluginRepository::installedPath()).filePath("sample"));

	// Local repositories are used in place: that is the development loop.
	QVERIFY(installed.isSymLink());
	QCOMPARE(QFileInfo(installed.symLinkTarget()).canonicalFilePath(),
		 QFileInfo(path + "/sample").canonicalFilePath());

	QList<PluginRepository::Installed> records = PluginRepository::installed();

	QCOMPARE(records.size(), 1);
	QCOMPARE(records[0].plugin, QString("sample"));
	QCOMPARE(records[0].version, QString("1.2.3"));
	QCOMPARE(records[0].repository, QString("install-from"));

	QCOMPARE(PluginRepository::available()[0].installedVersion, QString("1.2.3"));
}

void tst_pluginrepository::uninstallForgetsThePlugin()
{
	QString path = makeRepository("uninstall-from", "1.0.0");
	QString error;

	QVERIFY(PluginRepository::addRepository(path, &error));
	QVERIFY(PluginRepository::install("uninstall-from", "sample", &error));
	QVERIFY2(PluginRepository::uninstall("sample", &error), qPrintable(error));

	QVERIFY(!QFileInfo::exists(QDir(PluginRepository::installedPath()).filePath("sample")));
	QVERIFY(PluginRepository::installed().isEmpty());
	// The plugin's own files are untouched: it was only linked.
	QVERIFY(QFileInfo::exists(path + "/sample/plugin.json"));
}

void tst_pluginrepository::removingARepositoryLeavesInstalledPluginsAlone()
{
	QString path = makeRepository("removable", "1.0.0");
	QString error;

	QVERIFY(PluginRepository::addRepository(path, &error));
	QVERIFY(PluginRepository::install("removable", "sample", &error));
	QVERIFY2(PluginRepository::removeRepository("removable", &error), qPrintable(error));

	QVERIFY(PluginRepository::repositories().isEmpty());
	// Dropping the source does not uninstall what the user installed from it.
	QCOMPARE(PluginRepository::installed().size(), 1);
}

void tst_pluginrepository::clonesAGitRepositoryAndPinsItsRevision()
{
	if (!PluginRepository::gitAvailable())
		QSKIP("git is not installed");

	QString path = makeRepository("git-repo", "2.0.0");
	QString error;

	QVERIFY(git(QStringList() << "init" << "-q" << "-b" << "main", path));
	QVERIFY(commitAll(path, "initial"));

	QVERIFY2(PluginRepository::addRepository("file://" + path, &error), qPrintable(error));

	QList<PluginRepository::Repository> repositories = PluginRepository::repositories();

	QCOMPARE(repositories.size(), 1);
	QVERIFY(!repositories[0].local);
	QCOMPARE(repositories[0].revision.size(), 40);

	QVERIFY2(PluginRepository::install(repositories[0].id, "sample", &error), qPrintable(error));

	QFileInfo installed(QDir(PluginRepository::installedPath()).filePath("sample"));

	// A fetched repository is copied, not linked: the clone is FatRat's to
	// move around, the installed plugin is not.
	QVERIFY(installed.isDir());
	QVERIFY(!installed.isSymLink());
	QVERIFY(QFileInfo::exists(installed.filePath() + "/sample.py"));

	QList<PluginRepository::Installed> records = PluginRepository::installed();

	QCOMPARE(records[0].version, QString("2.0.0"));
	QCOMPARE(records[0].revision, repositories[0].revision);
}

void tst_pluginrepository::reportsAnUpdateAfterTheRepositoryMovesOn()
{
	if (!PluginRepository::gitAvailable())
		QSKIP("git is not installed");

	QString path = makeRepository("git-update", "1.0.0");
	QString error;

	QVERIFY(git(QStringList() << "init" << "-q" << "-b" << "main", path));
	QVERIFY(commitAll(path, "initial"));

	QVERIFY2(PluginRepository::addRepository("file://" + path, &error), qPrintable(error));

	QString id = PluginRepository::repositories()[0].id;

	QVERIFY2(PluginRepository::install(id, "sample", &error), qPrintable(error));
	QVERIFY(PluginRepository::updates().isEmpty());

	// The repository publishes a new version.
	QFile plugin(path + "/sample/plugin.json");

	plugin.open(QIODevice::WriteOnly | QIODevice::Truncate);
	plugin.write(R"({"schema": 1, "name": "sample", "version": "1.1.0", "api": "1", "entry": "sample.py"})");
	plugin.close();

	QVERIFY(commitAll(path, "bump"));

	// Until FatRat fetches, the user still sees what they installed.
	QVERIFY(PluginRepository::updates().isEmpty());

	QVERIFY2(PluginRepository::refresh(id, &error), qPrintable(error));

	QList<PluginRepository::Available> updates = PluginRepository::updates();

	QCOMPARE(updates.size(), 1);
	QCOMPARE(updates[0].plugin.version, QString("1.1.0"));
	QCOMPARE(updates[0].installedVersion, QString("1.0.0"));

	QVERIFY2(PluginRepository::install(id, "sample", &error), qPrintable(error));
	QCOMPARE(PluginRepository::installed()[0].version, QString("1.1.0"));
	QVERIFY(PluginRepository::updates().isEmpty());
}

QTEST_GUILESS_MAIN(tst_pluginrepository)
#include "tst_pluginrepository.moc"
