#include "App.h"
#include "ExtensionsDialog.h"
#include "Format.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

using namespace tui;

// A REST server that knows the plugin routes and records what it is sent
class FakeServer : public QObject
{
public:
	struct Request { QByteArray method, path, body; };

	explicit FakeServer(const QString& socketPath)
	{
		QVERIFY2(m_server.listen(socketPath), "cannot listen");
		connect(&m_server, &QLocalServer::newConnection, this, [this]() {
			QLocalSocket* s = m_server.nextPendingConnection();
			connect(s, &QLocalSocket::readyRead, this, [this, s]() {
				m_buf[s] += s->readAll();
				for (;;)
				{
					const int end = m_buf[s].indexOf("\r\n\r\n");
					if (end < 0)
						return;
					const QByteArray head = m_buf[s].left(end);
					int len = 0;
					for (const QByteArray& line : head.split('\n'))
						if (line.toLower().startsWith("content-length:"))
							len = line.mid(15).trimmed().toInt();
					if (m_buf[s].size() < end + 4 + len)
						return;
					const QByteArray body = m_buf[s].mid(end + 4, len);
					m_buf[s].remove(0, end + 4 + len);
					const QList<QByteArray> first = head.split('\r').first().split(' ');
					respond(s, { first[0], first[1], body });
				}
			});
		});
	}

	QVector<Request> requests;
	QJsonArray plugins, available, repositories;
	int installStatus = 201;
	QByteArray installError = "{\"error\":{\"code\":\"install_failed\",\"message\":\"pip said no\"}}";

	QVector<Request> of(const QByteArray& method, const QByteArray& path = QByteArray()) const
	{
		QVector<Request> r;
		for (const Request& q : requests)
			if (q.method == method && (path.isEmpty() || QUrl(QString::fromLatin1(q.path)).path().toLatin1() == path))
				r << q;
		return r;
	}

private:
	void reply(QLocalSocket* s, int status, const QByteArray& body = QByteArray())
	{
		s->write("HTTP/1.1 " + QByteArray::number(status) + " X\r\nContent-Type: application/json\r\nContent-Length: "
			 + QByteArray::number(body.size()) + "\r\n\r\n" + body);
	}

	void respond(QLocalSocket* s, const Request& r)
	{
		requests << r;

		const QUrl url(QString::fromLatin1(r.path));
		const QString path = url.path();

		if (r.method == "GET" && path == "/api/v1/plugins")
			return reply(s, 200, QJsonDocument(plugins).toJson(QJsonDocument::Compact));
		if (r.method == "GET" && path == "/api/v1/plugins/available")
			return reply(s, 200, QJsonDocument(available).toJson(QJsonDocument::Compact));
		if (r.method == "GET" && path == "/api/v1/plugin-repositories")
			return reply(s, 200, QJsonDocument(repositories).toJson(QJsonDocument::Compact));
		if (r.method == "POST" && path == "/api/v1/plugins")
		{
			if (installStatus >= 300)
				return reply(s, installStatus, installError);
			return reply(s, 201, "{\"plugin\":\"x\",\"restartRequired\":true}");
		}
		if (r.method == "PATCH" && path.startsWith("/api/v1/plugins/"))
			return reply(s, 200, "{\"name\":\"dummy\",\"valid\":true,\"enabled\":false}");
		if (r.method == "DELETE" && path.startsWith("/api/v1/plugins/"))
			return reply(s, 204);
		if (r.method == "POST" && path == "/api/v1/plugin-repositories")
			return reply(s, 201, "{\"id\":\"added\"}");
		if (r.method == "POST" && path == "/api/v1/plugin-repositories/refresh")
			return reply(s, 200, "[]");
		if (r.method == "DELETE" && path.startsWith("/api/v1/plugin-repositories/"))
			return reply(s, 204);

		reply(s, 404, "{\"error\":{\"code\":\"not_found\",\"message\":\"no\"}}");
	}

	QLocalServer m_server;
	QHash<QLocalSocket*, QByteArray> m_buf;
};

static QJsonObject plugin(const QString& name, const QString& version, bool enabled = true)
{
	return { { "name", name }, { "valid", true }, { "version", version }, { "enabled", enabled },
		 { "description", name + " does things" }, { "enabledKey", "python/plugins/" + name + "/enabled" } };
}

static QJsonObject offer(const QString& name, const QString& version, const QString& installed,
			 const QStringList& dependencies = QStringList())
{
	QJsonArray deps;
	for (const QString& d : dependencies)
		deps << d;

	return { { "name", name }, { "version", version }, { "repository", "example" },
		 { "installedVersion", installed }, { "description", "an example" }, { "dependencies", deps } };
}

class TestTuiExtensions : public QObject
{
	Q_OBJECT

	QTemporaryDir m_dir;
	std::unique_ptr<FakeServer> m_server;
	HttpClient* m_http = nullptr;
	ApiClient* m_api = nullptr;
	App* m_app = nullptr;
	std::unique_ptr<TextCanvas> m_canvas;

	QString render(int w = 110, int h = 34)
	{
		m_canvas.reset(new TextCanvas(w, h));
		m_app->draw(*m_canvas);
		return m_canvas->toText();
	}
	void key(int k, bool ctrl = false) { m_app->handle(Event::keyEvent(k, ctrl)); }
	void type(const QString& s) { for (QChar c : s) m_app->handle(Event::charEvent(c)); }

	void open()
	{
		key(KeyF4);
		QTRY_VERIFY_WITH_TIMEOUT(render().contains("Installed"), 5000);
	}
	// Tab until the wanted section is the open one (the open one is bracketed)
	void gotoSection(const QString& title)
	{
		for (int i = 0; i < 4; i++)
		{
			if (render().contains("[" + title))
				return;
			key(KeyTab);
		}
		QFAIL(qPrintable("cannot reach the section " + title));
	}

private slots:
	void init()
	{
		m_server.reset(new FakeServer(m_dir.path() + "/s.sock"));
		m_server->plugins = { plugin("dummy", "1.0.0"), plugin("extras", "1.0.0", false) };
		m_server->available = { offer("dummy", "1.1.0", "1.0.0"), offer("feedwatch", "1.0.0", "", { "feedparser" }) };
		m_server->repositories = {
			QJsonObject{ { "id", "example" }, { "name", "example-plugins" }, { "source", "/srv/plugins" },
				     { "local", true }, { "revision", "" }, { "description", "examples" } },
		};

		Glyphs::setAscii(false);
		m_http = new HttpClient;
		Endpoint ep;
		ep.kind = Endpoint::Unix;
		ep.socketPath = m_dir.path() + "/s.sock";
		m_http->setEndpoint(ep);
		m_api = new ApiClient(m_http);
		m_app = new App(m_api);
		m_app->setPaused(true);
		m_app->setConnected();
	}
	void cleanup()
	{
		delete m_app;
		delete m_api;
		delete m_http;
		m_server.reset();
	}

	void listsWhatIsInstalled()
	{
		open();

		const QString text = render();

		QVERIFY(text.contains("Extensions"));
		// The open section is bracketed, so a styleless terminal shows it too
		QVERIFY2(text.contains("[Installed (2)]"), qPrintable(text));
		QVERIFY(text.contains(" Available (2) "));
		QVERIFY(text.contains(" Repositories (1) "));
		QVERIFY(text.contains("dummy 1.0.0"));
		// The checkbox says which plugins are switched off
		QVERIFY(text.contains("[x] dummy"));
		QVERIFY(text.contains("[ ] extras"));
	}

	void showsABrokenPluginWithItsError()
	{
		m_server->plugins = { QJsonObject{ { "name", "broken" }, { "valid", false },
						   { "error", "plugin.json has no name" } } };
		open();

		const QString text = render();

		QVERIFY(text.contains("[!] broken"));
		QVERIFY2(text.contains("plugin.json has no name"), qPrintable(text));
	}

	void togglingAPluginPatchesItAndSaysWhenItApplies()
	{
		open();
		key('e');

		QTRY_VERIFY_WITH_TIMEOUT(!m_server->of("PATCH").isEmpty(), 5000);

		const FakeServer::Request r = m_server->of("PATCH").first();

		QCOMPARE(QUrl(QString::fromLatin1(r.path)).path(), QString("/api/v1/plugins/dummy"));
		QCOMPARE(QJsonDocument::fromJson(r.body).object().value("enabled").toBool(), false);
		QVERIFY(render().contains("next time FatRat starts"));
	}

	void removingAPluginAsksFirst()
	{
		open();
		key('d');

		QVERIFY(render().contains("Remove plugin"));
		QVERIFY(m_server->of("DELETE").isEmpty());

		key('n');
		QVERIFY(m_server->of("DELETE").isEmpty());

		key('d');
		key('y');
		QTRY_VERIFY_WITH_TIMEOUT(!m_server->of("DELETE").isEmpty(), 5000);
		QCOMPARE(QUrl(QString::fromLatin1(m_server->of("DELETE").first().path)).path(), QString("/api/v1/plugins/dummy"));
	}

	void marksAnUpdateAndInstallsIt()
	{
		open();
		gotoSection("Available");

		// "^" marks the entry whose repository has moved past what is installed
		QVERIFY2(render().contains("^ dummy 1.1.0"), qPrintable(render()));

		key(KeyEnter);

		QTRY_VERIFY_WITH_TIMEOUT(!m_server->of("POST", "/api/v1/plugins").isEmpty(), 5000);

		const QJsonObject body = QJsonDocument::fromJson(m_server->of("POST", "/api/v1/plugins").first().body).object();

		QCOMPARE(body.value("plugin").toString(), QString("dummy"));
		QCOMPARE(body.value("repository").toString(), QString("example"));
		QVERIFY(!body.contains("withDependencies"));
	}

	void namesThePypiPackagesBeforeInstallingThem()
	{
		open();
		gotoSection("Available");
		key(KeyDown);     // feedwatch, which needs feedparser
		key(KeyEnter);

		const QString text = render();

		QVERIFY2(text.contains("Install dependencies"), qPrintable(text));
		QVERIFY(text.contains("feedparser"));
		QVERIFY(text.contains("Python Package Index"));
		// Nothing is installed until the user says yes
		QVERIFY(m_server->of("POST", "/api/v1/plugins").isEmpty());

		key('y');
		QTRY_VERIFY_WITH_TIMEOUT(!m_server->of("POST", "/api/v1/plugins").isEmpty(), 5000);

		const QJsonObject body = QJsonDocument::fromJson(m_server->of("POST", "/api/v1/plugins").first().body).object();

		QCOMPARE(body.value("plugin").toString(), QString("feedwatch"));
		QCOMPARE(body.value("withDependencies").toBool(), true);
	}

	void declinedPackagesInstallNothing()
	{
		open();
		gotoSection("Available");
		key(KeyDown);
		key(KeyEnter);
		key('n');

		QVERIFY(m_server->of("POST", "/api/v1/plugins").isEmpty());
	}

	void aFailedInstallIsShownInTheWindow()
	{
		m_server->installStatus = 422;
		open();
		gotoSection("Available");
		key(KeyEnter);

		QTRY_VERIFY_WITH_TIMEOUT(render().contains("pip said no"), 5000);
	}

	void addingARepositoryWarnsAboutTrust()
	{
		open();
		gotoSection("Repositories");

		QVERIFY(render().contains("example-plugins"));
		QVERIFY(render().contains("local directory"));

		key('a');
		QVERIFY2(render().contains("Add plugin repository"), qPrintable(render()));

		type("https://example.com/p.git");
		key(KeyEnter);

		const QString text = render();

		QVERIFY2(text.contains("full privileges"), qPrintable(text));
		QVERIFY(text.contains("https://example.com/p.git"));
		// Nothing is fetched until the warning is accepted
		QVERIFY(m_server->of("POST", "/api/v1/plugin-repositories").isEmpty());

		key('y');
		QTRY_VERIFY_WITH_TIMEOUT(!m_server->of("POST", "/api/v1/plugin-repositories").isEmpty(), 5000);
		QCOMPARE(QJsonDocument::fromJson(m_server->of("POST", "/api/v1/plugin-repositories").first().body)
				 .object().value("source").toString(),
			 QString("https://example.com/p.git"));
	}

	void forgettingARepositoryAsksAndSaysPluginsStay()
	{
		open();
		gotoSection("Repositories");
		key('d');

		QVERIFY(render().contains("stay installed"));

		key('y');
		QTRY_VERIFY_WITH_TIMEOUT(!m_server->of("DELETE").isEmpty(), 5000);
		QCOMPARE(QUrl(QString::fromLatin1(m_server->of("DELETE").first().path)).path(),
			 QString("/api/v1/plugin-repositories/example"));
	}

	void refreshSaysSoWhenThereIsNothingToRefresh()
	{
		m_server->repositories = {};
		open();
		gotoSection("Repositories");
		key('r');

		QVERIFY(render().contains("No repositories"));
		QVERIFY(m_server->of("POST", "/api/v1/plugin-repositories/refresh").isEmpty());
	}

	void refreshesEveryRepository()
	{
		open();
		gotoSection("Repositories");
		key('r');

		QTRY_VERIFY_WITH_TIMEOUT(!m_server->of("POST", "/api/v1/plugin-repositories/refresh").isEmpty(), 5000);
	}

	void escapeClosesTheWindow()
	{
		open();
		key(KeyEscape);

		QVERIFY(!render().contains("Repositories ("));
	}

	void closingWhileACallIsInFlightIsSafe()
	{
		open();
		key('e');
		key(KeyEscape);

		// The reply arrives after the dialog is gone; nothing may touch it.
		QTest::qWait(300);
		QVERIFY(!render().contains("Repositories ("));
	}
};

QTEST_MAIN(TestTuiExtensions)
#include "tst_tui_extensions.moc"
