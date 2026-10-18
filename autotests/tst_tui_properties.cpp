#include "App.h"
#include "Canvas.h"
#include "Dialog.h"
#include "Dialogs.h"
#include "Format.h"
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

using namespace tui;

// A REST server that knows the transfer properties routes and records what it is sent
class PropsServer : public QObject
{
public:
	struct Request { QByteArray method, path, body; };

	explicit PropsServer(const QString& socketPath)
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
	// set by the tests
	QJsonObject options = QJsonDocument::fromJson(
		"{ \"fields\": [ { \"type\": \"text\", \"key\": \"fileName\", \"title\": \"Target file name\" },"
		"              { \"type\": \"number\", \"key\": \"seedUpload\", \"title\": \"Seed until uploaded\", \"min\": 0, \"suffix\": \"MB\" } ],"
		"  \"values\": { \"fileName\": \"a.bin\", \"seedUpload\": 0 } }").object();
	int optionsStatus = 200, patchStatus = 204, patchOptionsStatus = 200;
	QByteArray patchError = "{\"error\":{\"code\":\"invalid_property\",\"message\":\"Not a readable directory: /nope\"}}";
	QByteArray optionsError = "{\"error\":{\"code\":\"invalid_property\",\"message\":\"Invalid file name: \\\"\\\"\"}}";

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
		const QString path = QUrl(QString::fromLatin1(r.path)).path();
		if (r.method == "GET" && path.endsWith("/options"))
			return optionsStatus == 200 ? reply(s, 200, QJsonDocument(options).toJson(QJsonDocument::Compact))
						    : reply(s, optionsStatus, "{\"error\":{\"code\":\"boom\",\"message\":\"The server is broken\"}}");
		if (r.method == "PATCH" && path.endsWith("/options"))
			return patchOptionsStatus == 200 ? reply(s, 200, QJsonDocument(options).toJson(QJsonDocument::Compact))
							 : reply(s, patchOptionsStatus, optionsError);
		if (r.method == "PATCH" && path.startsWith("/api/v1/transfers/"))
			return patchStatus == 204 ? reply(s, 204) : reply(s, patchStatus, patchError);
		reply(s, 200, "[]");   // polling: queues, transfers, stats
	}

	QLocalServer m_server;
	QHash<QLocalSocket*, QByteArray> m_buf;
};

// FormDialog's async submit and the field operations a form filled in later needs. The dialog
// itself (Task 4) is tested further down in this file against a fake server.
class TestTuiProperties : public QObject
{
	Q_OBJECT

	static Event ev(int k) { return Event::keyEvent(k, false); }

	// a unix socket path must stay under ~100 bytes, which a build directory's temp dir may not
	QTemporaryDir m_dir{ "/tmp/tui-props-XXXXXX" };
	std::unique_ptr<PropsServer> m_server;
	HttpClient* m_http = nullptr;
	ApiClient* m_api = nullptr;
	App* m_app = nullptr;
	std::unique_ptr<TextCanvas> m_canvas;

	QString render(int w = 110, int h = 40)
	{
		m_canvas.reset(new TextCanvas(w, h));
		m_app->draw(*m_canvas);
		return m_canvas->toText();
	}
	void key(int k, bool ctrl = false) { m_app->handle(Event::keyEvent(k, ctrl)); }

	TransferInfo transfer(bool upload = false)
	{
		TransferInfo t;
		t.uuid = "{11111111-2222-3333-4444-555555555555}";
		t.name = "a.bin";
		t.object = "/downloads";
		t.comment = "c";
		t.primaryUpload = upload;
		t.upload = upload;
		return t;
	}
	FormDialog* m_dlg = nullptr;   // owned by the App
	void open(const TransferInfo& t)
	{
		m_dlg = dialogs::editTransfer(*m_app, t);
		QTRY_VERIFY_WITH_TIMEOUT(!render().contains("Loading options"), 5000);
	}
	// The focused field is only marked by colour, which the text canvas does not keep, so the
	// tests set fields through the dialog rather than by tabbing to them.
	void set(const QString& id, const QString& text) { m_dlg->focusField(id); m_dlg->setText(id, text); }

private slots:
	void init()
	{
		m_server.reset(new PropsServer(m_dir.path() + "/s.sock"));
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
		m_dlg = nullptr;
	}
	void cleanup()
	{
		delete m_app;
		delete m_api;
		delete m_http;
		m_server.reset();
	}

	void asyncSubmitWaitsForTheResult()
	{
		FormDialog d("Test", 60);
		d.addText("a", "A", "x");
		std::function<void(QString)> pending;
		int calls = 0;
		d.setSubmitAsync("Save", [&](FormDialog&, std::function<void(QString)> done) { calls++; pending = done; });

		d.handle(ev(KeyEnter));
		QCOMPARE(calls, 1);
		QVERIFY(!d.finished());
		QVERIFY(d.busy());

		// while waiting, neither another submit nor Esc does anything
		d.handle(ev(KeyEnter));
		d.handle(ev(KeyEscape));
		QCOMPARE(calls, 1);
		QVERIFY(!d.finished());

		pending(QString());
		QVERIFY(d.finished());
	}

	void asyncSubmitErrorKeepsTheDialogOpen()
	{
		FormDialog d("Test", 60);
		d.addText("a", "A", "x");
		d.setSubmitAsync("Save", [](FormDialog&, std::function<void(QString)> done) { done("Not a readable directory: /x"); });

		d.handle(ev(KeyEnter));
		QVERIFY(!d.finished());
		QVERIFY(!d.busy());
		QCOMPARE(d.error(), QString("Not a readable directory: /x"));
		QCOMPARE(d.text("a"), QString("x"));

		d.handle(ev(KeyEscape));   // usable again
		QVERIFY(d.finished());
	}

	void aLateResultAfterTheDialogIsGoneIsIgnored()
	{
		std::function<void(QString)> pending;
		{
			FormDialog d("Test", 60);
			d.setSubmitAsync("Save", [&](FormDialog&, std::function<void(QString)> done) { pending = done; });
			d.handle(ev(KeyEnter));
		}
		pending("too late");   // must not touch the destroyed dialog
	}

	void aStaleResultDoesNotSettleALaterSubmit()
	{
		FormDialog d("Test", 60);
		QVector<std::function<void(QString)>> dones;
		d.setSubmitAsync("Save", [&](FormDialog&, std::function<void(QString)> done) { dones.push_back(done); });

		d.handle(ev(KeyEnter));
		dones[0]("Refused");
		QCOMPARE(d.error(), QString("Refused"));

		d.handle(ev(KeyEnter));
		QCOMPARE(dones.size(), 2);
		dones[0](QString());   // the first submit reporting again
		QVERIFY(!d.finished());
		QVERIFY(d.busy());

		dones[1]("Refused again");
		dones[1](QString());   // done called twice
		QVERIFY(!d.finished());
		QCOMPARE(d.error(), QString("Refused again"));
	}

	void aClickAfterRemovingAFieldBeforeARedrawDoesNothing()
	{
		FormDialog d("Test", 60);
		d.addText("a", "A");   // not first: QList drops a front element without moving the rest
		d.addCheck("c1", "First", false);
		d.addCheck("c2", "Second", false);
		TextCanvas c(80, 24);
		HitMap hits;
		d.draw(c, hits, { 0, 0, 80, 24 });
		int row = -1, box = -1;
		for (int y = 0; y < 24 && row < 0; y++)
			if (c.line(y).contains("First"))
				row = y;
		for (int x = 0; x < 80 && box < 0 && row >= 0; x++)
			if (c.charAt(x, row) == "[")
				box = x;
		QVERIFY(box >= 0);

		d.removeField("c1");
		// the box of the removed field; its stale handler would toggle whatever now sits at its index
		d.handle(Event::mouseEvent(Event::Press, box + 1, row));
		QVERIFY(!d.checked("c2"));
	}

	void removingAnEmptyIdRemovesNothing()
	{
		FormPanel p;
		p.addLabel("Unnamed");
		p.addText("a", "A");
		const int before = p.contentHeight(40);
		p.removeField(QString());
		QCOMPARE(p.contentHeight(40), before);
	}

	void labelsCanBeNamedChangedAndRemoved()
	{
		FormPanel p;
		p.addText("a", "A");
		p.addLabel("Loading").id = "status";
		p.addText("b", "B");
		p.focusField("b");

		p.setText("status", "Failed");
		QCOMPARE(p.text("status"), QString("Failed"));

		p.removeField("status");
		QVERIFY(!p.field("status"));
		QCOMPARE(p.focusedId(), QString("b"));   // the focus stays on the same field
	}

	// ---- the Transfer properties dialog

	void showsTheOptionsAndTheRightPathLabel()
	{
		open(transfer());
		const QString s = render();
		QVERIFY(s.contains("Destination"));
		QVERIFY(s.contains("Run on completion"));
		QVERIFY(s.contains("%D destination directory"));
		QVERIFY(s.contains("Target file name"));
		QVERIFY(s.contains("a.bin"));
		QVERIFY(s.contains("MB"));
	}

	void anUploadHasASource()
	{
		open(transfer(true));
		QVERIFY(render().contains("Source"));
		QVERIFY(render().contains("%D source directory"));
	}

	void optionsThatCannotBeLoadedLeaveTheRestUsable()
	{
		m_server->optionsStatus = 500;
		m_dlg = dialogs::editTransfer(*m_app, transfer());
		QTRY_VERIFY_WITH_TIMEOUT(render().contains("Could not load the options"), 5000);
		set("comment", "new");
		key(KeyEnter);
		QTRY_VERIFY_WITH_TIMEOUT(!m_server->of("PATCH").isEmpty(), 5000);
	}

	void savesOnlyWhatChangedGeneralFirst()
	{
		open(transfer());
		set("command", "notify %N");
		set("opt:fileName", "b.bin");
		key(KeyEnter);
		QTRY_COMPARE_WITH_TIMEOUT(m_server->of("PATCH").size(), 2, 5000);
		const auto p = m_server->of("PATCH");
		QVERIFY(!p[0].path.endsWith("/options"));
		QCOMPARE(QJsonDocument::fromJson(p[0].body).object(), QJsonObject({ { "completionCommand", "notify %N" } }));
		QVERIFY(p[1].path.endsWith("/options"));
		QCOMPARE(QJsonDocument::fromJson(p[1].body).object(), QJsonObject({ { "fileName", "b.bin" } }));
		// App drops a dialog that a server reply closed on its next draw
		QTRY_VERIFY_WITH_TIMEOUT((render(), !m_app->hasDialog()), 5000);
	}

	void nothingChangedSendsNothing()
	{
		open(transfer());
		key(KeyEnter);
		QVERIFY(!m_app->hasDialog());
		QTest::qWait(200);
		QVERIFY(m_server->of("PATCH").isEmpty());
	}

	void aRefusedChangeKeepsTheDialogOpen()
	{
		m_server->patchStatus = 422;
		open(transfer());
		set("object", "/nope");
		key(KeyEnter);
		QTRY_VERIFY_WITH_TIMEOUT(render().contains("Not a readable directory: /nope"), 5000);
		QVERIFY(m_app->hasDialog());
		QVERIFY(render().contains("/nope"));   // what was typed is still there
	}

	void aRefusedOptionSaysTheOptionsWereNotSaved()
	{
		m_server->patchOptionsStatus = 422;
		open(transfer());
		set("opt:fileName", "x");
		key(KeyEnter);
		QTRY_VERIFY_WITH_TIMEOUT(render().contains("Options not saved"), 5000);
		QVERIFY(m_app->hasDialog());
	}

	void aBadNumberIsCaughtBeforeSending()
	{
		open(transfer());
		set("opt:seedUpload", "1.5");
		key(KeyEnter);
		QVERIFY(render().contains("Seed until uploaded must be a whole number"));
		QTest::qWait(200);
		QVERIFY(m_server->of("PATCH").isEmpty());
	}
};

QTEST_GUILESS_MAIN(TestTuiProperties)
#include "tst_tui_properties.moc"
