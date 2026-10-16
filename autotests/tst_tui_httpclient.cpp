#include "ApiClient.h"
#include "HttpClient.h"
#include <QLocalServer>
#include <QLocalSocket>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include "ServerManager.h"
#include <QtTest>
#include <sys/stat.h>

using namespace tui;

class TestTuiHttp : public QObject
{
	Q_OBJECT

	static HttpResponse run(HttpClient& c, const QByteArray& method, const QString& path, const QByteArray& body = QByteArray())
	{
		HttpResponse out;
		bool done = false;
		c.request(method, path, [&](const HttpResponse& r) { out = r; done = true; }, body);
		for (int i = 0; i < 100 && !done; i++)
			QTest::qWait(50);
		return out;
	}

private slots:
	void parserContentLength()
	{
		HttpParser p;
		QVERIFY(!p.feed("HTTP/1.1 200 OK\r\nContent-Len"));
		QVERIFY(!p.feed("gth: 5\r\nContent-Type: text/plain\r\n\r\nhe"));
		QVERIFY(p.feed("llo"));
		QCOMPARE(p.response().status, 200);
		QCOMPARE(p.response().body, QByteArray("hello"));
		QCOMPARE(p.response().headers["content-type"], QByteArray("text/plain"));
		QVERIFY(p.keepAlive());
	}
	void parserChunked()
	{
		HttpParser p;
		QVERIFY(p.feed("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n4\r\nWiki\r\n5;ext=1\r\npedia\r\n0\r\n\r\n"));
		QCOMPARE(p.response().body, QByteArray("Wikipedia"));
	}
	void parserChunkedByteByByte()
	{
		const QByteArray raw = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nabc\r\n0\r\n\r\n";
		HttpParser p;
		bool done = false;
		for (char c : raw)
			done = p.feed(QByteArray(1, c));
		QVERIFY(done);
		QCOMPARE(p.response().body, QByteArray("abc"));
	}
	void parserNoContent()
	{
		HttpParser p;
		QVERIFY(p.feed("HTTP/1.1 204 No Content\r\n\r\n"));
		QCOMPARE(p.response().status, 204);
	}
	void parserUntilClose()
	{
		HttpParser p;
		QVERIFY(!p.feed("HTTP/1.0 200 OK\r\n\r\nabc"));
		QVERIFY(p.finishOnClose());
		QCOMPARE(p.response().body, QByteArray("abc"));
		QVERIFY(!p.keepAlive());
	}
	void parserGarbage()
	{
		HttpParser p;
		p.feed("this is not http\r\n\r\n");
		QVERIFY(p.failed());
	}
	void endpointParsing()
	{
		Endpoint e;
		QVERIFY(Endpoint::parse("https://example.org:5010", e));
		QCOMPARE(e.kind, Endpoint::Tls);
		QCOMPARE(e.port, quint16(5010));
		QVERIFY(Endpoint::parse("http://example.org", e));
		QCOMPARE(e.port, quint16(80));
		QCOMPARE(e.hostHeader(), QString("example.org"));
		QVERIFY(Endpoint::parse("unix:/run/user/1000/fatrat/rest.sock", e));
		QCOMPARE(e.kind, Endpoint::Unix);
		QCOMPARE(e.socketPath, QString("/run/user/1000/fatrat/rest.sock"));
		QVERIFY(!Endpoint::parse("ftp://x", e));
		QVERIFY(!Endpoint::parse("nonsense", e));
	}

	void overUnixSocket()
	{
		QTemporaryDir dir;
		QLocalServer srv;
		const QString path = dir.path() + "/s.sock";
		QVERIFY(srv.listen(path));

		QByteArray lastRequest;
		int connections = 0;
		connect(&srv, &QLocalServer::newConnection, [&]() {
			QLocalSocket* s = srv.nextPendingConnection();
			connections++;
			connect(s, &QLocalSocket::readyRead, [&, s]() {
				lastRequest += s->readAll();
				if (!lastRequest.contains("\r\n\r\n"))
					return;
				const QByteArray body = "{\"total\":{\"down\":1,\"up\":2},\"queues\":{}}";
				s->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
				lastRequest.clear();
			});
		});

		HttpClient c;
		Endpoint ep;
		ep.kind = Endpoint::Unix;
		ep.socketPath = path;
		c.setEndpoint(ep);
		ApiClient api(&c);

		for (int i = 0; i < 3; i++)
		{
			ApiResult res;
			bool done = false;
			api.stats([&](const ApiResult& r) { res = r; done = true; });
			QTRY_VERIFY_WITH_TIMEOUT(done, 5000);
			QVERIFY(res.ok());
			QCOMPARE(res.json.toObject()["total"].toObject()["up"].toInt(), 2);
		}
		QCOMPARE(connections, 1); // keep-alive: one connection for all three
	}

	void queuesRequestsInOrder()
	{
		QTcpServer srv;
		QVERIFY(srv.listen(QHostAddress::LocalHost));
		QByteArray buf;
		connect(&srv, &QTcpServer::newConnection, [&]() {
			QTcpSocket* s = srv.nextPendingConnection();
			connect(s, &QTcpSocket::readyRead, [&, s]() {
				buf += s->readAll();
				int end;
				while ((end = buf.indexOf("\r\n\r\n")) >= 0)
				{
					const QByteArray line = buf.left(buf.indexOf("\r\n"));
					buf.remove(0, end + 4);
					s->write("HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(line.size()) + "\r\n\r\n" + line);
				}
			});
		});

		HttpClient c;
		Endpoint ep;
		ep.kind = Endpoint::Tcp;
		ep.host = "127.0.0.1";
		ep.port = srv.serverPort();
		ep.authorization = "Basic eDpwdw==";
		c.setEndpoint(ep);

		QStringList got;
		for (const char* p : { "/a", "/b", "/c" })
			c.request("GET", p, [&](const HttpResponse& r) { got << QString::fromLatin1(r.body); });
		QTRY_COMPARE_WITH_TIMEOUT(got.size(), 3, 5000);
		QCOMPARE(got[0], QString("GET /a HTTP/1.1"));
		QCOMPARE(got[2], QString("GET /c HTTP/1.1"));
	}

	void connectionRefusedReportsError()
	{
		HttpClient c;
		Endpoint ep;
		ep.kind = Endpoint::Tcp;
		ep.host = "127.0.0.1";
		ep.port = 1; // nothing listens here
		c.setEndpoint(ep);
		HttpResponse r = run(c, "GET", "/");
		QCOMPARE(r.status, 0);
		QVERIFY(!r.error.isEmpty());
	}

	void postIsNotResentOnAFreshConnection()
	{
		QTcpServer srv;
		QVERIFY(srv.listen(QHostAddress::LocalHost));
		int requests = 0;
		connect(&srv, &QTcpServer::newConnection, [&]() {
			QTcpSocket* s = srv.nextPendingConnection();
			connect(s, &QTcpSocket::readyRead, [&, s]() {
				if (s->readAll().contains("POST"))
					requests++;
				s->close(); // the server dies after receiving the request, without an answer
			});
		});
		HttpClient c;
		Endpoint ep;
		ep.kind = Endpoint::Tcp;
		ep.host = "127.0.0.1";
		ep.port = srv.serverPort();
		c.setEndpoint(ep);
		HttpResponse r = run(c, "POST", "/x", "{}");
		QCOMPARE(r.status, 0);
		QTest::qWait(200);
		QCOMPARE(requests, 1); // a duplicate "add transfer" would be wrong
	}

	void staleKeepAliveConnectionIsRetried()
	{
		QTcpServer srv;
		QVERIFY(srv.listen(QHostAddress::LocalHost));
		int connections = 0;
		connect(&srv, &QTcpServer::newConnection, [&]() {
			QTcpSocket* s = srv.nextPendingConnection();
			const int n = ++connections;
			connect(s, &QTcpSocket::readyRead, [n, s]() {
				s->readAll();
				if (n == 1)
				{
					s->write("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok");
					QTimer::singleShot(50, s, [s]() { s->close(); }); // idle timeout of the server
				}
				else
					s->write("HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nnew");
			});
		});
		HttpClient c;
		Endpoint ep;
		ep.kind = Endpoint::Tcp;
		ep.host = "127.0.0.1";
		ep.port = srv.serverPort();
		c.setEndpoint(ep);
		QCOMPARE(run(c, "GET", "/a").body, QByteArray("ok"));
		QTest::qWait(300); // the server has closed the idle connection by now
		QCOMPARE(run(c, "GET", "/b").body, QByteArray("new"));
	}

	void changingTheEndpointDoesNotWedgeTheQueue()
	{
		QTcpServer slow, fast;
		QVERIFY(slow.listen(QHostAddress::LocalHost));
		QVERIFY(fast.listen(QHostAddress::LocalHost));
		connect(&fast, &QTcpServer::newConnection, [&]() {
			QTcpSocket* s = fast.nextPendingConnection();
			connect(s, &QTcpSocket::readyRead, [s]() { s->readAll(); s->write("HTTP/1.1 200 OK\r\nContent-Length: 1\r\n\r\nx"); });
		});
		HttpClient c;
		Endpoint ep;
		ep.kind = Endpoint::Tcp;
		ep.host = "127.0.0.1";
		ep.port = slow.serverPort(); // accepts but never answers
		c.setEndpoint(ep);
		HttpResponse first;
		bool firstDone = false;
		c.request("GET", "/a", [&](const HttpResponse& r) { first = r; firstDone = true; });
		QTest::qWait(100);
		ep.port = fast.serverPort();
		c.setEndpoint(ep);
		QVERIFY(firstDone);
		QCOMPARE(first.status, 0);
		QCOMPARE(run(c, "GET", "/b").body, QByteArray("x"));
	}

	void socketDirectoryMustBePrivate()
	{
		QTemporaryDir dir;
		QString err;
		// a directory that does not exist is created with mode 0700
		QVERIFY(ServerManager::prepareSocketDirectory(dir.path() + "/new", &err));
		struct stat st;
		QCOMPARE(stat(qPrintable(dir.path() + "/new"), &st), 0);
		QCOMPARE(int(st.st_mode & 0777), 0700);
		// an existing directory that others can enter is refused, and left alone
		QVERIFY(QDir().mkdir(dir.path() + "/open"));
		chmod(qPrintable(dir.path() + "/open"), 0755);
		QVERIFY(!ServerManager::prepareSocketDirectory(dir.path() + "/open", &err));
		QVERIFY(!err.isEmpty());
		QCOMPARE(stat(qPrintable(dir.path() + "/open"), &st), 0);
		QCOMPARE(int(st.st_mode & 0777), 0755);
	}

	void apiErrorBodyIsParsed()
	{
		QTemporaryDir dir;
		QLocalServer srv;
		QVERIFY(srv.listen(dir.path() + "/s.sock"));
		connect(&srv, &QLocalServer::newConnection, [&]() {
			QLocalSocket* s = srv.nextPendingConnection();
			connect(s, &QLocalSocket::readyRead, [s]() {
				s->readAll();
				const QByteArray body = "{\"error\":{\"code\":\"queue_not_empty\",\"message\":\"Queue still has transfers\"}}";
				s->write("HTTP/1.1 409 Conflict\r\nContent-Type: application/json\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
			});
		});
		HttpClient c;
		Endpoint ep;
		ep.kind = Endpoint::Unix;
		ep.socketPath = dir.path() + "/s.sock";
		c.setEndpoint(ep);
		ApiClient api(&c);
		ApiResult res;
		bool done = false;
		api.deleteQueue("q1", DeleteQueueOptions(), [&](const ApiResult& r) { res = r; done = true; });
		QTRY_VERIFY_WITH_TIMEOUT(done, 5000);
		QCOMPARE(res.status, 409);
		QCOMPARE(res.errorCode, QString("queue_not_empty"));
		QCOMPARE(res.errorMessage, QString("Queue still has transfers"));
	}
};

QTEST_MAIN(TestTuiHttp)
#include "tst_tui_httpclient.moc"
