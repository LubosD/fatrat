#include "App.h"
#include "NcBackend.h"
#include "Script.h"
#include "ServerManager.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QSocketNotifier>
#include <QStandardPaths>
#include <QTimer>
#include <csignal>
#include <cstdio>
#include <unistd.h>

using namespace tui;

namespace
{
	int g_signalPipe[2] = { -1, -1 };

	void onSignal(int)
	{
		const char c = 1;
		if (write(g_signalPipe[1], &c, 1) < 0)
			return;
	}

	// Shown while the local server starts
	class Splash : public Dialog
	{
	public:
		Splash(ServerManager* sm, std::function<void()> cancel) : m_sm(sm), m_cancel(std::move(cancel)) {}

		void draw(Canvas& c, HitMap& hits, const Rect& screen) override
		{
			beginDraw(hits, screen);
			m_rect = place(screen, 70, 9 + m_sm->steps().size());
			drawFrame(c, "fatrat-tui");
			c.put(m_rect.x + 3, m_rect.y + 2, QObject::tr("Starting a local FatRat server…"), Style::Bold);
			int y = m_rect.y + 4;
			for (const QString& s : m_sm->steps())
				c.put(m_rect.x + 3, y++, fit(Glyphs::isAscii() ? "* " + s : QString::fromUtf8("✓ ") + s, m_rect.w - 6), Style::Normal);
			c.put(m_rect.x + 3, y, QObject::tr("… waiting for the API"), Style::Yellow);
			c.put(m_rect.x + 3, m_rect.bottom() - 3, fit(QObject::tr("Server log: %1").arg(m_sm->options().logPath), m_rect.w - 6), Style::Dim);
			drawHints(c, m_rect.x + 1, m_rect.bottom() - 2, m_rect.w - 2, { { "Esc", QObject::tr("Quit"), m_cancel } });
		}

		void handle(const Event& e) override
		{
			if (e.type == Event::Key && (e.is(KeyEscape) || e.isLetter('q')))
				m_cancel();
			else
				handleMouse(e);
		}
		void outsideClick() override {}

	private:
		static QString fit(const QString& s, int w) { return s.left(w); }
		ServerManager* m_sm;
		std::function<void()> m_cancel;
	};
}

int main(int argc, char** argv)
{
	QCoreApplication qapp(argc, argv);
	QCoreApplication::setApplicationName("fatrat-tui");

	QCommandLineParser p;
	p.setApplicationDescription("Terminal client for FatRat. Without --url it starts (or reuses) a local FatRat server on a unix socket.");
	p.addHelpOption();
	p.addOption({ "url", "Connect to a FatRat web interface: http://host:port, https://host:port or unix:/path/to.sock", "URL" });
	p.addOption({ "password-file", "Read the web interface password from the first line of FILE", "FILE" });
	p.addOption({ "password-prompt", "Ask for the password even if FATRAT_PASSWORD or --password-file is set" });
	p.addOption({ "insecure", "Do not verify the server certificate" });
	p.addOption({ "socket", "Unix socket of the local server (default: $XDG_RUNTIME_DIR/fatrat/rest.sock)", "PATH" });
	p.addOption({ "server-bin", "The FatRat program to start (default: fatrat-nogui or fatrat next to this program, or in PATH)", "PATH" });
	p.addOption({ "config", "Settings file for the local server", "FILE" });
	p.addOption({ "stop", "Stop a local server that was started by fatrat-tui, then exit" });
	p.addOption({ "no-mouse", "Do not use the mouse" });
	p.addOption({ "ascii", "Draw with plain ASCII characters" });
	p.addOption({ "script", "Run the commands of FILE against a virtual screen and print the results (for tests)", "FILE" });
	p.addOption({ "size", "Screen size of --script, as COLSxROWS (default 110x34)", "SIZE" });
	p.process(qapp);

	const QString socketPath = p.value("socket").isEmpty() ? ServerManager::defaultSocketPath() : p.value("socket");

	if (p.isSet("stop"))
	{
		bool ok;
		const QString msg = ServerManager::stopRunning(socketPath, &ok);
		fprintf(stderr, "%s\n", qPrintable(msg));
		return ok ? 0 : 1;
	}

	const bool scripted = p.isSet("script");

	// ---- connection settings
	QSettings conf(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/fatrat/tui.conf", QSettings::IniFormat);
	Endpoint ep;
	QString urlText = p.value("url");
	if (urlText.isEmpty() && qEnvironmentVariableIsSet("FATRAT_URL"))
		urlText = qEnvironmentVariable("FATRAT_URL");
	const bool urlMode = !urlText.isEmpty();
	QString desc;

	if (urlMode)
	{
		QString err;
		if (!Endpoint::parse(urlText, ep, &err))
		{
			fprintf(stderr, "%s\n", qPrintable(err));
			return 2;
		}
		if (QUrl(urlText).userInfo().size())
			fprintf(stderr, "Ignoring the user name and password in the URL; use --password-file or FATRAT_PASSWORD.\n");
		ep.insecure = p.isSet("insecure");
		{
			QUrl shown(urlText);
			shown.setUserInfo(QString()); // never display a password that was put in the URL
			desc = shown.toString();
		}
		for (const QString& fp : conf.value("fingerprints/" + ep.host + ":" + QString::number(ep.port)).toStringList())
			ep.trustedFingerprints.insert(fp);
	}
	else
	{
		ep.kind = Endpoint::Unix;
		ep.socketPath = socketPath;
		desc = QObject::tr("local server");
	}

	// ---- password (URL mode over TCP)
	QString password;
	bool havePassword = false;
	if (urlMode && ep.kind != Endpoint::Unix && !p.isSet("password-prompt"))
	{
		if (p.isSet("password-file"))
		{
			QFile f(p.value("password-file"));
			if (f.open(QIODevice::ReadOnly) && (f.permissions() & (QFileDevice::ReadGroup | QFileDevice::ReadOther | QFileDevice::WriteGroup | QFileDevice::WriteOther)) == 0)
			{
				password = QString::fromUtf8(f.readLine()).trimmed();
				havePassword = true;
			}
			else
			{
				fprintf(stderr, "Cannot use the password file %s (missing, or readable by others).\n", qPrintable(p.value("password-file")));
				return 2;
			}
		}
		else if (qEnvironmentVariableIsSet("FATRAT_PASSWORD"))
		{
			password = qEnvironmentVariable("FATRAT_PASSWORD");
			havePassword = true;
		}
	}
	auto authFor = [](const QString& pw) { return "Basic " + QByteArray(":" + pw.toUtf8()).toBase64(); };
	if (havePassword)
		ep.authorization = authFor(password);

	// ---- terminal
	if (!scripted && (!isatty(0) || !isatty(1)))
	{
		fprintf(stderr, "fatrat-tui needs a terminal.\n");
		return 1;
	}
	setlocale(LC_ALL, "");
	Glyphs::setAscii(p.isSet("ascii") || !QString(setlocale(LC_CTYPE, nullptr)).contains("UTF", Qt::CaseInsensitive));
	NcBackend nc;
	if (!scripted && !nc.init(!p.isSet("no-mouse")))
	{
		fprintf(stderr, "Cannot initialise the terminal.\n");
		return 1;
	}

	HttpClient http;
	http.setEndpoint(ep);
	ApiClient api(&http);
	App app(&api);
	app.setDescription(desc);
	if (ep.kind == Endpoint::Tcp && ep.host != "localhost" && ep.host != "127.0.0.1" && ep.host != "::1")
		app.toast(QObject::tr("Plain HTTP: the password travels unencrypted. Use https:// if the server offers it."), true);

	if (pipe(g_signalPipe) == 0)
	{
		signal(SIGTERM, onSignal);
		signal(SIGHUP, onSignal);
		auto* sn = new QSocketNotifier(g_signalPipe[0], QSocketNotifier::Read, &qapp);
		QObject::connect(sn, &QSocketNotifier::activated, &qapp, [&]() { app.quitNow(); });
	}

	QObject::connect(&app, &App::quit, &qapp, &QCoreApplication::quit);

	// ---- URL mode: password and certificate prompts
	QObject::connect(&http, &HttpClient::certificateUntrusted, &app, [&](const QString& subject, const QString& fp, const QStringList& problems) {
		app.askCertificate(subject, fp, problems);
	});
	app.certificateDecision = [&](bool remember) {
		if (!remember)
			return;
		const QString key = "fingerprints/" + http.endpoint().host + ":" + QString::number(http.endpoint().port);
		QStringList l = conf.value(key).toStringList();
		for (const QString& fp : http.endpoint().trustedFingerprints)
			if (!l.contains(fp))
				l << fp;
		conf.setValue(key, l);
		conf.sync();
	};
	app.passwordEntered = [&](const QString& pw) {
		Endpoint e = http.endpoint();
		e.authorization = authFor(pw);
		http.setEndpoint(e);
		app.setPaused(false);
	};

	// ---- local server
	ServerManager* sm = nullptr;
	if (!urlMode)
	{
		ServerManager::Options o;
		o.socketPath = socketPath;
		o.serverBinary = p.value("server-bin");
		o.configFile = p.value("config");
		sm = new ServerManager(o, &app);
		app.setServer(sm);
		app.setPaused(true);

		auto startServer = std::make_shared<std::function<void()>>();
		auto splash = std::make_shared<Splash*>(nullptr);
		*startServer = [&, startServer, splash]() {
			auto s = std::make_unique<Splash>(sm, [&]() { sm->stop(); app.requestQuit(); });
			*splash = s.get();
			app.push(std::move(s));
			QTimer::singleShot(0, sm, &ServerManager::start);
		};

		QObject::connect(sm, &ServerManager::ready, &app, [&, splash]() {
			app.dismiss(*splash);
			*splash = nullptr;
			app.setPaused(false);
			app.refreshSoon();
		});
		QObject::connect(sm, &ServerManager::failed, &app, [&, startServer, splash](const QString& msg) {
			app.dismiss(*splash);
			*splash = nullptr;
			QVector<Choice> ch = { { "R", QObject::tr("Retry"), 1 }, { "Q", QObject::tr("Quit"), 0 } };
			app.push(std::make_unique<ChoiceDialog>(QObject::tr("Cannot start the server"), msg.split('\n'), ch, [&, startServer](int r) {
				if (r == 1)
					(*startServer)();
				else
					app.requestQuit();
			}));
		});
		(*startServer)();
	}
	else if (!havePassword && ep.kind != Endpoint::Unix)
	{
		app.setPaused(true);
		app.askPassword();
	}

	// ---- scripted run: no terminal
	if (scripted)
	{
		int cols = 110, rows = 34;
		const QStringList sz = p.value("size").split('x');
		if (sz.size() == 2)
		{
			cols = sz[0].toInt();
			rows = sz[1].toInt();
		}
		QFile f(p.value("script"));
		if (!f.open(QIODevice::ReadOnly))
		{
			fprintf(stderr, "Cannot read %s\n", qPrintable(p.value("script")));
			return 2;
		}
		TextCanvas canvas(cols, rows);
		ScriptRunner runner(app, canvas, QString::fromUtf8(f.readAll()).split('\n'));
		int rc = 0;
		QObject::connect(&runner, &ScriptRunner::finished, &qapp, [&](int code) { rc = code; qapp.quit(); });
		runner.start();
		qapp.exec();
		if (sm)
			sm->stop();
		return rc;
	}

	// ---- main loop
	auto redraw = [&]() {
		nc.begin();
		app.draw(nc);
		nc.flush();
	};

	auto* in = new QSocketNotifier(0, QSocketNotifier::Read, &qapp);
	QObject::connect(in, &QSocketNotifier::activated, &qapp, [&]() {
		std::vector<Event> events;
		nc.readEvents(events);
		// a readable stdin that never yields input means the terminal is gone
		static int emptyWakeups = 0;
		emptyWakeups = events.empty() ? emptyWakeups + 1 : 0;
		if (emptyWakeups > 20)
		{
			app.quitNow();
			return;
		}
		for (const Event& e : events)
		{
			if (e.type == Event::Key && e.ctrl && e.key == 'c')
				app.requestQuit();
			else
				app.handle(e);
		}
		redraw();
	});

	QTimer ticker;
	QObject::connect(&ticker, &QTimer::timeout, &qapp, redraw);
	ticker.start(250);
	redraw();

	const int rc = qapp.exec();
	nc.shutdown();
	return rc;
}
