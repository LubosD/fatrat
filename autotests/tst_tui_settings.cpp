#include "App.h"
#include "Format.h"
#include "SettingsDialog.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QUrl>
#include <QUrlQuery>
#include <QtTest>

using namespace tui;

// A REST server that knows just the settings routes and records what it is sent
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
	QJsonObject values;
	QJsonArray pages;
	int pagesStatus = 200;

	QVector<Request> of(const QByteArray& method) const
	{
		QVector<Request> r;
		for (const Request& q : requests)
			if (q.method == method)
				r << q;
		return r;
	}

private:
	void reply(QLocalSocket* s, int status, const QByteArray& body = QByteArray())
	{
		s->write("HTTP/1.1 " + QByteArray::number(status) + " X\r\nContent-Type: application/json\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
	}

	void respond(QLocalSocket* s, const Request& r)
	{
		requests << r;
		const QUrl url(QString::fromLatin1(r.path));
		if (r.method == "GET" && url.path() == "/api/v1/settings/pages")
		{
			if (pagesStatus != 200)
				return reply(s, pagesStatus, "{\"error\":{\"code\":\"boom\",\"message\":\"The server is broken\"}}");
			return reply(s, 200, QJsonDocument(pages).toJson(QJsonDocument::Compact));
		}
		if (r.method == "GET" && url.path() == "/api/v1/settings")
		{
			QJsonObject out;
			for (const QString& k : QUrlQuery(url).allQueryItemValues("key", QUrl::FullyDecoded))
				out[k] = values.value(k);
			return reply(s, 200, QJsonDocument(out).toJson(QJsonDocument::Compact));
		}
		if (r.method == "PATCH" && url.path() == "/api/v1/settings")
			return reply(s, 204);
		if (r.method == "POST" && url.path() == "/api/v1/settings/apply")
			return reply(s, 202);
		if (r.method == "POST" && url.path() == "/api/v1/settings/web-interface/certificate")
			return reply(s, 200, "{\"path\":\"/tmp/fatrat-webui.pem\"}");
		reply(s, 404, "{\"error\":{\"code\":\"not_found\",\"message\":\"no\"}}");
	}

	QLocalServer m_server;
	QHash<QLocalSocket*, QByteArray> m_buf;
};

static QJsonObject page(const QString& title, const QString& script)
{
	return { { "title", title }, { "script", script }, { "icon", "/img/x.png" } };
}

class TestTuiSettings : public QObject
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
	void openSettings()
	{
		key('o');
		QTRY_VERIFY_WITH_TIMEOUT(render().contains("BitTorrent"), 5000);
	}
	// Replaces the text of the focused field
	void setField(const QString& text)
	{
		key('u', true);
		type(text);
	}

private slots:
	void init()
	{
		m_server.reset(new FakeServer(m_dir.path() + "/s.sock"));
		m_server->pages = { page("BitTorrent", "/scripts/settings/bittorrent.js"), page("RSS", "/scripts/settings/rss.js"),
			page("Web interface", "/scripts/settings/webinterface.js"), page("Mystery", "/scripts/settings/mystery.js") };
		m_server->values = {
			{ "torrent/listen_start", 6881 }, { "torrent/listen_end", 6999 }, { "torrent/maxratio", 1.5 }, { "torrent/maxconnections", 200 },
			{ "torrent/maxuploads", 4 }, { "torrent/maxconnections_loc", 50 }, { "torrent/maxuploads_loc", 2 }, { "torrent/maxfiles", 20 },
			{ "torrent/dht", true }, { "torrent/pex", false }, { "torrent/allocation", 1 }, { "torrent/external_ip", "" },
			{ "torrent/enc_incoming", 1 }, { "torrent/enc_outgoing", 2 }, { "torrent/enc_level", 0 }, { "torrent/enc_rc4_prefer", false },
			{ "torrent/mapping_upnp", true }, { "torrent/mapping_natpmp", false }, { "torrent/mapping_lsd", "true" },
			{ "rss/enable", true }, { "rss/interval", 15 },
			{ "remote/enable", true }, { "remote/port", 8080 }, { "remote/password", "secret" }, { "remote/ssl", false }, { "remote/ssl_pem", "" },
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

	// ---- the pure part

	void unchangedFormsProduceNoChanges()
	{
		for (const char* script : { "/scripts/settings/bittorrent.js", "/scripts/settings/rss.js", "/scripts/settings/webinterface.js" })
		{
			const SettingsPageSpec* spec = settingsPageSpec(script);
			QVERIFY(spec);
			FormPanel panel;
			buildSettingsPanel(*spec, m_server->values, panel);
			QJsonObject changes;
			QString error;
			QVERIFY2(collectSettings(*spec, panel, m_server->values, changes, &error, nullptr), qPrintable(error));
			QVERIFY2(changes.isEmpty(), script);
		}
		QVERIFY(!settingsPageSpec("/scripts/settings/mystery.js"));
	}

	void valuesThatCameBackAsTextAreUnderstood()
	{
		// a setting read from an ini file may be text
		QJsonObject v = { { "rss/enable", "true" }, { "rss/interval", "20" } };
		const SettingsPageSpec* spec = settingsPageSpec("/scripts/settings/rss.js");
		FormPanel panel;
		buildSettingsPanel(*spec, v, panel);
		QVERIFY(panel.checked("rss/enable"));
		QCOMPARE(panel.text("rss/interval"), QString("20"));
		QJsonObject changes;
		QVERIFY(collectSettings(*spec, panel, v, changes, nullptr, nullptr));
		QVERIFY(changes.isEmpty());
	}

	void onlyChangedValuesAreCollected()
	{
		const SettingsPageSpec* spec = settingsPageSpec("/scripts/settings/bittorrent.js");
		FormPanel panel;
		buildSettingsPanel(*spec, m_server->values, panel);
		panel.setText("torrent/maxratio", "2.5");
		panel.field("torrent/allocation")->index = 2;
		panel.field("torrent/dht")->checked = false;
		QJsonObject changes;
		QVERIFY(collectSettings(*spec, panel, m_server->values, changes, nullptr, nullptr));
		QCOMPARE(changes.size(), 3);
		QCOMPARE(changes["torrent/maxratio"].toDouble(), 2.5);
		QCOMPARE(changes["torrent/allocation"].toInt(), 2);
		QCOMPARE(changes["torrent/dht"].toBool(), false);
	}

	void badValuesAreRefused()
	{
		const SettingsPageSpec* spec = settingsPageSpec("/scripts/settings/bittorrent.js");
		struct Case { const char* key; const char* text; const char* message; };
		for (const Case& c : { Case{ "torrent/listen_start", "80", "at least 1024" }, Case{ "torrent/listen_end", "70000", "at most 65535" },
			Case{ "torrent/maxconnections", "x", "whole number" }, Case{ "torrent/maxratio", "-1", "at least" }, Case{ "torrent/maxfiles", "", "whole number" } })
		{
			FormPanel panel;
			buildSettingsPanel(*spec, m_server->values, panel);
			panel.setText(c.key, c.text);
			QJsonObject changes;
			QString error, bad;
			QVERIFY2(!collectSettings(*spec, panel, m_server->values, changes, &error, &bad), c.key);
			QCOMPARE(bad, QString(c.key));
			QVERIFY2(error.contains(c.message), qPrintable(error));
		}
	}

	// ---- the dialog

	void showsThePagesAndTheFirstForm()
	{
		openSettings();
		const QString t = render();
		QVERIFY(t.contains("RSS"));
		QVERIFY(t.contains("Web interface"));
		QVERIFY(t.contains("Listen port (from)"));
		QVERIFY(t.contains("6881"));
		QVERIFY(t.contains("File allocation mode"));
		QVERIFY(t.contains("Sparse"));
		QCOMPARE(m_server->of("GET").size(), 2);
	}

	void tabBarHasASettingsItem()
	{
		QVERIFY(render().contains("O Settings"));
		int x = -1, y = -1;
		for (const Hit& h : m_app->hits().hits())
			if (h.action == Action::Settings)
			{
				x = h.rect.x;
				y = h.rect.y;
			}
		QVERIFY(x >= 0);
		m_app->handle(Event::mouseEvent(Event::Press, x, y));
		QTRY_VERIFY_WITH_TIMEOUT(render().contains("Listen port (from)"), 5000);
	}

	void switchingPagesShowsTheirForms()
	{
		openSettings();
		key(KeyDown);
		QVERIFY(render().contains("Check every"));
		QVERIFY(render().contains("minutes"));
		key(KeyDown);
		QVERIFY(render().contains("Remote web control"));
		// the password is not shown
		QVERIFY(!render().contains("secret"));
		key(KeyDown);
		QVERIFY(render().contains("isn't supported in this UI yet"));
	}

	void editingAndSavingSendsOnlyTheChange()
	{
		openSettings();
		key(KeyEnter);               // into the form, on the first text field
		setField("7000");
		key(KeyEnter);               // OK
		QTRY_VERIFY_WITH_TIMEOUT(!m_server->of("PATCH").isEmpty() && !m_server->of("POST").isEmpty(), 5000);
		const auto patch = QJsonDocument::fromJson(m_server->of("PATCH").first().body).object();
		QCOMPARE(patch.size(), 1);
		QCOMPARE(patch["torrent/listen_start"].toInt(), 7000);
		QCOMPARE(m_server->of("POST").first().path, QByteArray("/api/v1/settings/apply"));
		QTRY_VERIFY_WITH_TIMEOUT((render(), !m_app->hasDialog()), 5000);
	}

	void savingWithoutChangesJustCloses()
	{
		openSettings();
		key(KeyEnter);
		key(KeyEnter);
		QVERIFY(!m_app->hasDialog());
		QTest::qWait(100);
		QVERIFY(m_server->of("PATCH").isEmpty());
	}

	void anInvalidValueStaysOnTheDialogAndFocusesTheField()
	{
		openSettings();
		key(KeyEnter);
		setField("80");
		key(KeyEnter);
		QVERIFY(m_app->hasDialog());
		QVERIFY(render().contains("must be at least 1024"));
		QTest::qWait(100);
		QVERIFY(m_server->of("PATCH").isEmpty());
		// fix it
		setField("2000");
		key(KeyEnter);
		QTRY_VERIFY_WITH_TIMEOUT(!m_server->of("PATCH").isEmpty(), 5000);
	}

	void editsOfSeveralPagesAreSavedTogether()
	{
		openSettings();
		key(KeyEnter);
		setField("7000");
		key(KeyPageDown);            // RSS: the focus is on its checkbox
		QCOMPARE(m_app->hasDialog(), true);
		key(' ');                    // untick "Check the feeds"
		key(KeyPageUp);              // the first page still has what was typed
		QVERIFY(render().contains("7000"));
		key(KeyEnter);
		QTRY_VERIFY_WITH_TIMEOUT(!m_server->of("PATCH").isEmpty(), 5000);
		const auto patch = QJsonDocument::fromJson(m_server->of("PATCH").first().body).object();
		QCOMPARE(patch.size(), 2);
		QCOMPARE(patch["torrent/listen_start"].toInt(), 7000);
		QCOMPARE(patch["rss/enable"].toBool(), true == false);
	}

	void cancelAsksOnlyWhenThereAreChanges()
	{
		openSettings();
		key(KeyEscape);              // nothing changed
		QVERIFY(!m_app->hasDialog());

		openSettings();
		key(KeyEnter);
		setField("7000");
		key(KeyEscape);              // back to the page list
		QVERIFY(m_app->hasDialog());
		key(KeyEscape);              // cancel: asks
		QVERIFY(render().contains("Discard the changes?"));
		key('k');                    // keep editing
		QVERIFY(render().contains("Listen port (from)"));
		QVERIFY(!render().contains("Discard the changes?"));
		key(KeyEscape);
		key('d');
		QVERIFY(!m_app->hasDialog());
		QTest::qWait(100);
		QVERIFY(m_server->of("PATCH").isEmpty());
	}

	void theCompactLayoutShowsTheListThenThePage()
	{
		key('o');
		QTRY_VERIFY_WITH_TIMEOUT(render(90, 30).contains("BitTorrent"), 5000);
		QString t = render(90, 30);
		QVERIFY(t.contains("Web interface"));
		QVERIFY(!t.contains("Listen port (from)"));
		key(KeyEnter);
		t = render(90, 30);
		QVERIFY(t.contains("Settings > BitTorrent"));
		QVERIFY(t.contains("Listen port (from)"));
		QVERIFY(!t.contains("Web interface"));
		key(KeyEscape);              // back to the list, not out of the dialog
		QVERIFY(render(90, 30).contains("Web interface"));
		QVERIFY(m_app->hasDialog());
	}

	void tabLeavesTheFormAtItsEnds()
	{
		openSettings();
		key(KeyEnter);
		key(KeyBackTab);             // on the first field: back to the list
		key(KeyDown);                // so this moves the page, not a field
		QVERIFY(render().contains("Check every"));
	}

	void webInterfacePageGeneratesACertificate()
	{
		openSettings();
		key(KeyDown);
		key(KeyDown);
		key(KeyEnter);
		key('g', true);
		QVERIFY(render().contains("Generate a self-signed certificate"));
		key(KeyEnter);               // the default host
		QTRY_VERIFY_WITH_TIMEOUT(render().contains("/tmp/fatrat-webui.pem"), 5000);
		const auto req = m_server->of("POST");
		QCOMPARE(req.last().path, QByteArray("/api/v1/settings/web-interface/certificate"));
		QCOMPARE(QJsonDocument::fromJson(req.last().body).object()["hostname"].toString(), QString("localhost"));
		// it becomes part of what OK saves
		key(KeyEnter);
		QTRY_VERIFY_WITH_TIMEOUT(!m_server->of("PATCH").isEmpty(), 5000);
		QCOMPARE(QJsonDocument::fromJson(m_server->of("PATCH").first().body).object()["remote/ssl_pem"].toString(), QString("/tmp/fatrat-webui.pem"));
	}

	void aServerErrorIsShown()
	{
		m_server->pagesStatus = 500;
		key('o');
		QTRY_VERIFY_WITH_TIMEOUT(render().contains("Cannot load the settings"), 5000);
		QVERIFY(render().contains("The server is broken"));
		key(KeyEscape);
		QVERIFY(!m_app->hasDialog());
	}

	// TUI_DUMP=1 ./tst_tui_settings dump  prints the screens
	void dump()
	{
		if (!qEnvironmentVariableIsSet("TUI_DUMP"))
			QSKIP("only with TUI_DUMP");
		openSettings();
		puts(qPrintable(render(110, 34)));
		key(KeyEnter);
		for (int i = 0; i < 14; i++)
			key(KeyDown);
		puts(qPrintable(render(110, 24)));
		key(KeyPageDown);
		key(KeyPageDown);
		puts(qPrintable(render(110, 30)));
		puts(qPrintable(render(80, 24)));
	}

	void closingBeforeTheAnswerIsSafe()
	{
		key('o');
		key(KeyEscape);              // the pages have not arrived yet
		QVERIFY(!m_app->hasDialog());
		QTest::qWait(200);
		render();
	}
};

QTEST_MAIN(TestTuiSettings)
#include "tst_tui_settings.moc"
