#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include "python/PluginRegistry.h"
#include "python/PluginRepository.h"
#include "remote/RestApi.h"
#include "remote/RestResources.h"

/* The /plugins and /plugin-repositories routes: status codes, JSON shape and
 * the error envelope, against a repository in a temp directory.
 *
 * PluginRegistry's three lookups are stubbed below -- the real one imports
 * plugins into a live interpreter, which is not what this is testing.
 */
static QStringList s_disabled;

QList<PluginRegistry::Manifest> PluginRegistry::installed()
{
	return PluginManifest::readAll(PluginRepository::installedPath(), "1");
}

QString PluginRegistry::enabledKey(const QString& name)
{
	return "python/plugins/" + name + "/enabled";
}

bool PluginRegistry::isPluginEnabled(const QString& name)
{
	return !s_disabled.contains(name);
}

void PluginRegistry::setPluginEnabled(const QString& name, bool enabled)
{
	s_disabled.removeAll(name);

	if (!enabled)
		s_disabled << name;
}

class tst_restplugins : public QObject
{
	Q_OBJECT
private slots:
	void initTestCase();
	void init();

	void listsNoRepositoriesByDefault();
	void addsARepositoryAndListsWhatItOffers();
	void rejectsARepositoryThatIsNotOne();
	void installsListsDisablesAndRemovesAPlugin();
	void reportsAnUnknownPlugin();
private:
	RestResponse call(const QString& method, const QString& path, const QByteArray& body = QByteArray());
	QJsonValue json(const RestResponse& response);
	QString makeRepository();

	QTemporaryDir m_dir;
	QString m_repository;
};

void tst_restplugins::initTestCase()
{
	RestApi::clearRoutes();
	RestResources::registerPlugins();

	m_repository = makeRepository();
}

void tst_restplugins::init()
{
	static int counter = 0;

	QString root = QDir(m_dir.path()).filePath(QString("root%1").arg(++counter));

	QDir().mkpath(root);
	PluginRepository::setRoot(root);
	s_disabled.clear();
}

QString tst_restplugins::makeRepository()
{
	QString path = QDir(m_dir.path()).filePath("repo");

	QDir().mkpath(path + "/sample");

	QFile repository(path + "/fatrat-plugins.json");

	repository.open(QIODevice::WriteOnly);
	repository.write(R"({"schema": 1, "name": "sample-repo", "description": "test",
		"plugins": [{"name": "sample", "source": "./sample", "description": "a sample"}]})");
	repository.close();

	QFile plugin(path + "/sample/plugin.json");

	plugin.open(QIODevice::WriteOnly);
	plugin.write(R"({"schema": 1, "name": "sample", "version": "1.0.0", "api": "1",
		"entry": "sample.py", "description": "a sample",
		"config": [{"type": "text", "key": "sample/user", "title": "Username"}]})");
	plugin.close();

	QFile module(path + "/sample/sample.py");

	module.open(QIODevice::WriteOnly);
	module.write("# sample\n");
	module.close();

	return path;
}

RestResponse tst_restplugins::call(const QString& method, const QString& path, const QByteArray& body)
{
	RestRequest request;

	request.method = method;
	request.path = QString("/api/v1") + path;
	request.body = body;
	request.contentType = "application/json";
	// The CSRF guard wants a same-origin request.
	request.host = "localhost";
	request.origin = "http://localhost";

	return RestApi::dispatch(request);
}

QJsonValue tst_restplugins::json(const RestResponse& response)
{
	return QJsonDocument::fromJson(response.body).isArray()
		? QJsonValue(QJsonDocument::fromJson(response.body).array())
		: QJsonValue(QJsonDocument::fromJson(response.body).object());
}

void tst_restplugins::listsNoRepositoriesByDefault()
{
	RestResponse response = call("GET", "/plugin-repositories");

	QCOMPARE(response.status, 200);
	QCOMPARE(json(response).toArray().size(), 0);
}

void tst_restplugins::addsARepositoryAndListsWhatItOffers()
{
	RestResponse added = call("POST", "/plugin-repositories",
				  QJsonDocument(QJsonObject{{"source", m_repository}}).toJson());

	QCOMPARE(added.status, 201);
	QCOMPARE(json(added).toObject().value("name").toString(), QString("sample-repo"));
	QVERIFY(json(added).toObject().value("local").toBool());

	RestResponse available = call("GET", "/plugins/available");
	QJsonArray entries = json(available).toArray();

	QCOMPARE(available.status, 200);
	QCOMPARE(entries.size(), 1);
	QCOMPARE(entries[0].toObject().value("name").toString(), QString("sample"));
	QCOMPARE(entries[0].toObject().value("installedVersion").toString(), QString());
}

void tst_restplugins::rejectsARepositoryThatIsNotOne()
{
	QString empty = QDir(m_dir.path()).filePath("not-a-repo");

	QDir().mkpath(empty);

	RestResponse response = call("POST", "/plugin-repositories",
				     QJsonDocument(QJsonObject{{"source", empty}}).toJson());

	QCOMPARE(response.status, 422);
	QCOMPARE(json(response).toObject().value("error").toObject().value("code").toString(),
		 QString("repository_rejected"));
}

void tst_restplugins::installsListsDisablesAndRemovesAPlugin()
{
	call("POST", "/plugin-repositories", QJsonDocument(QJsonObject{{"source", m_repository}}).toJson());

	RestResponse installed = call("POST", "/plugins",
		QJsonDocument(QJsonObject{{"repository", "repo"}, {"plugin", "sample"}}).toJson());

	QCOMPARE(installed.status, 201);
	// Engines register at startup, and the client is told so.
	QVERIFY(json(installed).toObject().value("restartRequired").toBool());

	RestResponse list = call("GET", "/plugins");
	QJsonArray plugins = json(list).toArray();

	QCOMPARE(plugins.size(), 1);

	QJsonObject plugin = plugins[0].toObject();

	QCOMPARE(plugin.value("name").toString(), QString("sample"));
	QCOMPARE(plugin.value("version").toString(), QString("1.0.0"));
	QVERIFY(plugin.value("enabled").toBool());
	// The settings page travels as data, which is what lets the web UI
	// render the same page the desktop does.
	QCOMPARE(plugin.value("config").toArray().size(), 1);
	QCOMPARE(plugin.value("config").toArray()[0].toObject().value("key").toString(),
		 QString("sample/user"));

	RestResponse disabled = call("PATCH", "/plugins/sample",
				     QJsonDocument(QJsonObject{{"enabled", false}}).toJson());

	QCOMPARE(disabled.status, 200);
	QVERIFY(!json(disabled).toObject().value("enabled").toBool());

	RestResponse rejected = call("PATCH", "/plugins/sample",
				     QJsonDocument(QJsonObject{{"nonsense", true}}).toJson());

	QCOMPARE(rejected.status, 422);

	RestResponse removed = call("DELETE", "/plugins/sample");

	QCOMPARE(removed.status, 204);
	QCOMPARE(json(call("GET", "/plugins")).toArray().size(), 0);
}

void tst_restplugins::reportsAnUnknownPlugin()
{
	RestResponse response = call("GET", "/plugins/nope");

	QCOMPARE(response.status, 404);
	QCOMPARE(json(response).toObject().value("error").toObject().value("code").toString(),
		 QString("plugin_not_found"));
}

QTEST_GUILESS_MAIN(tst_restplugins)
#include "tst_restplugins.moc"
