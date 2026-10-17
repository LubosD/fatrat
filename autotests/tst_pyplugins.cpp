// Python headers before Qt; see src/python/PyRef.h.
#include "PyPython.h"

#include <pybind11/eval.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

#include "python/PyPlugin.h"
#include "python/PyVM.h"

namespace py = pybind11;

/* A one-shot HTTP server: enough for a plugin to fetch from, nothing more. */
class TestServer : public QTcpServer
{
	Q_OBJECT
public:
	QByteArray lastBody, lastRequestLine, lastHeaders;

	QString url(const QString& path) const
	{
		return QString("http://127.0.0.1:%1%2").arg(serverPort()).arg(path);
	}
protected:
	virtual void incomingConnection(qintptr handle)
	{
		QTcpSocket* socket = new QTcpSocket(this);
		socket->setSocketDescriptor(handle);

		connect(socket, &QTcpSocket::readyRead, this, [this, socket]()
		{
			QByteArray request = socket->readAll();
			int headerEnd = request.indexOf("\r\n\r\n");
			if (headerEnd < 0)
				return; // wait for the rest

			lastRequestLine = request.left(request.indexOf("\r\n"));
			lastHeaders = request.mid(0, headerEnd);
			lastBody = request.mid(headerEnd + 4);

			QByteArray body, status = "200 OK", extra;

			if (lastRequestLine.contains("/hello"))
				body = "hello <b>world</b>";
			else if (lastRequestLine.contains("/echo"))
				body = lastBody;
			else if (lastRequestLine.contains("/json"))
			{
				body = "{\"link\": \"http://example.com/f.bin\"}";
				extra = "Content-Type: application/json\r\n";
			}
			else
			{
				status = "404 Not Found";
				body = "nope";
			}

			QByteArray response = "HTTP/1.1 " + status + "\r\n"
				+ extra
				+ "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
				"Connection: close\r\n\r\n" + body;

			socket->write(response);
			socket->flush();
			socket->disconnectFromHost();
		});
	}
};

class tst_pyplugins : public QObject
{
	Q_OBJECT
private slots:
	void initTestCase();
	void cleanupTestCase();

	void fetchResolvesWithResponse();
	void fetchPostSendsForm();
	void fetchFailureRaisesNetworkError();
	void pluginExceptionIsReported();
	void taskCancellationRaisesInPlugin();
	void settingsRoundTrip();
	void runInThreadResolves();
	void waitResolvesAfterCountdown();
	void captchaResolvesWithSolution();
	void unsolvedCaptchaRaises();
	void pluginLoaderImportsADirectory();
	void pluginLoaderReloadsEditedCodeAndForgetsTheOld();
private:
	// Runs `source` (which must define `main(plugin, server_url)` as a
	// coroutine function), waits for it, and reports what came back.
	// Void, because the QTRY_* macros it uses can only return from void.
	void runPlugin(const QString& source, const QString& path, QString* result, QString* error);

	PyVM* m_vm;
	TestServer* m_server;
};

void tst_pyplugins::initTestCase()
{
	qputenv("FATRAT_PYTHON_PATH", FATRAT_PYTHON_RUNTIME);

	m_vm = new PyVM;
	QVERIFY2(PyVM::available(), "the embedded interpreter did not come up");

	m_server = new TestServer;
	QVERIFY(m_server->listen(QHostAddress::LocalHost));
}

void tst_pyplugins::cleanupTestCase()
{
	delete m_server;
	m_server = 0;
	delete m_vm;
	m_vm = 0;
}

void tst_pyplugins::runPlugin(const QString& source, const QString& path, QString* result, QString* error)
{
	bool done = false;
	QString resultText, errorText;

	{
		py::gil_scoped_acquire gil;

		py::dict globals;
		globals["__builtins__"] = py::module_::import("builtins");

		py::exec(source.toStdString(), globals);

		py::object plugin = py::module_::import("fatrat").attr("Plugin")();
		py::object coroutine = globals["main"](plugin, m_server->url(path).toStdString());

		pySpawnTask(coroutine, [&done, &resultText, &errorText](py::object value, py::object exc)
		{
			if (!exc.is_none())
				errorText = QString::fromStdString(py::str(exc).cast<std::string>());
			else if (!value.is_none())
				resultText = QString::fromStdString(py::str(value).cast<std::string>());
			done = true;
		});
	}

	// Spin the event loop: the fetch completes in a Qt slot, which resumes
	// the coroutine.
	QTRY_VERIFY_WITH_TIMEOUT(done, 5000);

	if (result)
		*result = resultText;
	if (error)
		*error = errorText;
}

void tst_pyplugins::fetchResolvesWithResponse()
{
	QString result, error;
	runPlugin(
		"async def main(plugin, url):\n"
		"    page = await plugin.fetch(url)\n"
		"    assert page.status == 200, page.status\n"
		"    return page.search(r'<b>(\\w+)</b>')\n",
		"/hello", &result, &error);

	QVERIFY2(error.isEmpty(), qPrintable(error));
	QCOMPARE(result, QString("world"));
}

void tst_pyplugins::fetchPostSendsForm()
{
	QString result, error;
	runPlugin(
		"async def main(plugin, url):\n"
		"    page = await plugin.fetch(url, data={'token': 'abc', 'id': '7'})\n"
		"    return page.text\n",
		"/echo", &result, &error);

	QVERIFY2(error.isEmpty(), qPrintable(error));
	QCOMPARE(result, QString("token=abc&id=7"));
	QVERIFY(m_server->lastRequestLine.startsWith("POST "));
	QVERIFY(m_server->lastHeaders.contains("Content-Type: application/x-www-form-urlencoded"));
}

void tst_pyplugins::fetchFailureRaisesNetworkError()
{
	QString result, error;
	runPlugin(
		"async def main(plugin, url):\n"
		"    await plugin.fetch('http://127.0.0.1:1/nothing-is-listening')\n"
		"    return 'should not get here'\n",
		"/hello", &result, &error);

	QVERIFY2(error.contains("NetworkError") || error.contains("refused"), qPrintable(error));
}

void tst_pyplugins::pluginExceptionIsReported()
{
	QString result, error;
	runPlugin(
		"async def main(plugin, url):\n"
		"    page = await plugin.fetch(url)\n"
		"    raise ValueError('the page changed layout')\n",
		"/hello", &result, &error);

	QVERIFY2(error.contains("layout"), qPrintable(error));
}

void tst_pyplugins::taskCancellationRaisesInPlugin()
{
	bool done = false;
	QString errorText;
	py::object task;

	{
		py::gil_scoped_acquire gil;

		py::dict globals;
		globals["__builtins__"] = py::module_::import("builtins");
		py::exec(
			"import fatrat\n"
			"async def main(plugin, url):\n"
			"    try:\n"
			"        await plugin.fetch(url)\n"
			"    except fatrat.CancelledError:\n"
			"        raise RuntimeError('stopped cleanly')\n",
			globals);

		py::object plugin = py::module_::import("fatrat").attr("Plugin")();
		py::object coroutine = globals["main"](plugin, m_server->url("/hello").toStdString());

		task = pySpawnTask(coroutine, [&done, &errorText](py::object, py::object exc)
		{
			if (!exc.is_none())
				errorText = QString::fromStdString(py::str(exc).cast<std::string>());
			done = true;
		});

		task.attr("cancel")();
	}

	QTRY_VERIFY_WITH_TIMEOUT(done, 5000);
	QCOMPARE(errorText, QString("stopped cleanly"));
}

void tst_pyplugins::settingsRoundTrip()
{
	py::gil_scoped_acquire gil;

	py::module_ fatrat = py::module_::import("fatrat");
	py::object settings = fatrat.attr("settings");

	settings.attr("__setitem__")("example/user", py::str("someone"));
	settings.attr("__setitem__")("example/retries", py::int_(3));

	QCOMPARE(QString::fromStdString(settings.attr("get")("example/user").cast<std::string>()), QString("someone"));
	QCOMPARE(settings.attr("get")("example/retries").cast<int>(), 3);
	QVERIFY(settings.attr("get")("example/missing").is_none());
	QCOMPARE(settings.attr("get")("example/missing", py::str("fallback")).cast<std::string>(), std::string("fallback"));
}

void tst_pyplugins::runInThreadResolves()
{
	bool done = false;
	QString resultText, errorText;

	{
		py::gil_scoped_acquire gil;

		py::dict globals;
		globals["__builtins__"] = py::module_::import("builtins");
		py::exec(
			"import fatrat\n"
			"def work():\n"
			"    return sum(range(1000))\n"
			"async def main():\n"
			"    return await fatrat.run_in_thread(work)\n",
			globals);

		pySpawnTask(globals["main"](), [&done, &resultText, &errorText](py::object value, py::object exc)
		{
			if (!exc.is_none())
				errorText = QString::fromStdString(py::str(exc).cast<std::string>());
			else
				resultText = QString::fromStdString(py::str(value).cast<std::string>());
			done = true;
		});
	}

	QTRY_VERIFY_WITH_TIMEOUT(done, 5000);
	QVERIFY2(errorText.isEmpty(), qPrintable(errorText));
	QCOMPARE(resultText, QString("499500"));
}

void tst_pyplugins::waitResolvesAfterCountdown()
{
	QString result, error;
	QElapsedTimer elapsed;

	elapsed.start();
	runPlugin(
		"async def main(plugin, url):\n"
		"    await plugin.wait(1)\n"
		"    return 'waited'\n",
		"/hello", &result, &error);

	QVERIFY2(error.isEmpty(), qPrintable(error));
	QCOMPARE(result, QString("waited"));
	QVERIFY2(elapsed.elapsed() >= 900, "the countdown returned too early");
}

void tst_pyplugins::captchaResolvesWithSolution()
{
	QString result, error;
	runPlugin(
		"async def main(plugin, url):\n"
		"    return await plugin.solve_captcha('http://example.com/captcha.png')\n",
		"/hello", &result, &error);

	QVERIFY2(error.isEmpty(), qPrintable(error));
	QCOMPARE(result, QString("XYZZY"));
}

void tst_pyplugins::unsolvedCaptchaRaises()
{
	QString result, error;
	runPlugin(
		"async def main(plugin, url):\n"
		"    return await plugin.solve_captcha('http://example.com/unsolvable.png')\n",
		"/hello", &result, &error);

	QVERIFY2(error.contains("captcha"), qPrintable(error));
}

/* The loader is what turns an installed directory into registered plugin
 * classes; everything after it is the engine's job.
 */
void tst_pyplugins::pluginLoaderImportsADirectory()
{
	py::gil_scoped_acquire gil;

	py::object descriptors = py::module_::import("fatrat._loader").attr("load")(
		"dummy", EXAMPLE_PLUGIN_DIR, "dummy.py");

	QCOMPARE(int(py::len(descriptors)), 1);

	py::handle descriptor = descriptors[py::int_(0)];

	QCOMPARE(QString::fromStdString(py::str(descriptor.attr("kind")).cast<std::string>()), QString("download"));
	QCOMPARE(QString::fromStdString(py::str(descriptor.attr("identifier")).cast<std::string>()),
		 QString("fatrat_plugins.dummy.Dummy"));
	QVERIFY(descriptor.attr("options")["single_transfer"].cast<int>() == 0);
}

void tst_pyplugins::pluginLoaderReloadsEditedCodeAndForgetsTheOld()
{
	QTemporaryDir dir;
	QVERIFY(dir.isValid());

	auto write = [&](const QString& name, const QByteArray& content) {
		QFile f(QDir(dir.path()).filePath(name));
		QVERIFY(f.open(QIODevice::WriteOnly));
		f.write(content);
	};

	write("helper.py", "VALUE = 1\n");
	write("reloadme.py",
		"import fatrat, helper\n"
		"@fatrat.search_plugin(name='V' + str(helper.VALUE))\n"
		"class P(fatrat.SearchPlugin):\n"
		"    pass\n");

	py::gil_scoped_acquire gil;

	py::module_ loader = py::module_::import("fatrat._loader");
	py::module_ sys = py::module_::import("sys");
	auto nameOf = [&]() {
		py::object descriptors = loader.attr("load")("reloadme", dir.path().toStdString(), "reloadme.py");
		return QString::fromStdString(py::str(descriptors[py::int_(0)].attr("name")).cast<std::string>());
	};

	QCOMPARE(nameOf(), QString("V1"));
	QVERIFY(sys.attr("modules").contains("helper"));
	QVERIFY(sys.attr("path").attr("__contains__")(dir.path().toStdString()).cast<bool>());

	// Edit both the plugin's helper and the plugin, as a developer would.
	// Python decides a cached .pyc is stale by mtime and size, so make
	// sure the new file differs.
	write("helper.py", "VALUE = 22\n");

	QCOMPARE(nameOf(), QString("V22"));

	// Unloading forgets the modules, the path entry and the registration.
	int before = int(py::len(py::module_::import("fatrat.registry").attr("registered")()));

	loader.attr("unload")("reloadme");

	QVERIFY(!sys.attr("modules").contains("helper"));
	QVERIFY(!sys.attr("modules").contains("fatrat_plugins.reloadme"));
	QVERIFY(!sys.attr("path").attr("__contains__")(dir.path().toStdString()).cast<bool>());
	QCOMPARE(int(py::len(py::module_::import("fatrat.registry").attr("registered")())), before - 1);
}

QTEST_GUILESS_MAIN(tst_pyplugins)
#include "tst_pyplugins.moc"
