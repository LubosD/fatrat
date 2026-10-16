#include "Connection.h"
#include "ServerManager.h"
#include <QFileInfo>
#include <QObject>
#include <QSettings>
#include <QTcpSocket>
#include <QUrl>

namespace tui
{

LocalInstance readLocalInstance(const QString& settingsFile)
{
	LocalInstance li;
	QSettings native(QSettings::NativeFormat, QSettings::UserScope, "Dolezel", "fatrat");
	QSettings custom(settingsFile, QSettings::IniFormat);
	QSettings& s = settingsFile.isEmpty() ? native : custom;

	li.file = settingsFile.isEmpty() ? native.fileName() : settingsFile;
	li.fileFound = QFileInfo::exists(li.file);
	li.enabled = s.value("remote/enable", false).toBool();
	li.port = quint16(s.value("remote/port", 2233).toUInt());
	li.ssl = s.value("remote/ssl", false).toBool();
	li.password = s.value("remote/password", QString()).toString();
	return li;
}

bool tcpPortAnswers(const QString& host, quint16 port, int timeoutMs)
{
	QTcpSocket s;
	s.connectToHost(host, port);
	const bool ok = s.waitForConnected(timeoutMs);
	s.abort();
	return ok;
}

bool parseMode(const QString& s, PlanOptions::Mode* out)
{
	const QString m = s.toLower();
	if (m == "auto") *out = PlanOptions::Auto;
	else if (m == "socket" || m == "private") *out = PlanOptions::Socket;
	else if (m == "local") *out = PlanOptions::Local;
	else if (m == "url") *out = PlanOptions::Url;
	else return false;
	return true;
}

QString modeName(PlanOptions::Mode m)
{
	switch (m)
	{
		case PlanOptions::Socket: return "socket";
		case PlanOptions::Local: return "local";
		case PlanOptions::Url: return "url";
		default: return "auto";
	}
}

Plan planConnection(const PlanOptions& o)
{
	Plan p;
	auto socketAnswers = o.socketAnswers ? o.socketAnswers : [](const QString& path) { return ServerManager::probe(path); };
	auto portAnswers = o.portAnswers ? o.portAnswers : [](const QString& host, quint16 port) { return tcpPortAnswers(host, port); };
	const QString sock = o.socketPath.isEmpty() ? ServerManager::defaultSocketPath() : o.socketPath;

	// ---- an address given explicitly
	if (o.mode == PlanOptions::Url || !o.url.isEmpty())
	{
		QString err;
		if (o.url.isEmpty() || !Endpoint::parse(o.url, p.endpoint, &err))
		{
			p.error = o.url.isEmpty() ? QObject::tr("--mode url needs --url") : err;
			return p;
		}
		QUrl shown(o.url);
		shown.setUserInfo(QString()); // never display a password that was put in the URL
		p.kind = Plan::Url;
		p.description = p.endpoint.kind == Endpoint::Unix ? QObject::tr("server on %1").arg(p.endpoint.socketPath) : shown.toString();
		p.explanation << QObject::tr("Using the address you gave: %1").arg(p.description);
		return p;
	}

	const bool wantSocket = o.mode == PlanOptions::Auto || o.mode == PlanOptions::Socket;
	const bool wantLocal = o.mode == PlanOptions::Auto || o.mode == PlanOptions::Local;
	QStringList tried;

	// ---- 1. a server on the unix socket that is already running
	if (wantSocket)
	{
		if (socketAnswers(sock))
		{
			p.kind = Plan::ExistingSocket;
			p.endpoint.kind = Endpoint::Unix;
			p.endpoint.socketPath = sock;
			p.description = QObject::tr("server on the unix socket");
			p.explanation << QObject::tr("Unix socket %1: a server is running, using it").arg(sock);
			return p;
		}
		tried << QObject::tr("Unix socket %1: nothing is listening").arg(sock);
	}

	// ---- 2. a running FatRat whose settings enable the web interface
	if (wantLocal)
	{
		const LocalInstance li = readLocalInstance(o.settingsFile);
		p.settingsFile = li.file;
		if (!li.enabled)
		{
			tried << (li.fileFound ? QObject::tr("Settings %1: the web interface is turned off").arg(li.file)
				: QObject::tr("Settings %1: not found").arg(li.file));
		}
		else if (!portAnswers("127.0.0.1", li.port))
		{
			tried << QObject::tr("Settings %1: the web interface is on port %2, but nothing answers there (FatRat is not running)").arg(li.file).arg(li.port);
		}
		else
		{
			p.kind = Plan::LocalInstance;
			p.endpoint.kind = li.ssl ? Endpoint::Tls : Endpoint::Tcp;
			p.endpoint.host = "127.0.0.1";
			p.endpoint.port = li.port;
			p.password = li.password;
			p.passwordFromSettings = !li.password.isEmpty();
			p.description = QObject::tr("running FatRat, %1://127.0.0.1:%2").arg(li.ssl ? "https" : "http").arg(li.port);
			p.explanation << tried;
			p.explanation << QObject::tr("Settings %1: the web interface is on port %2 and answers, using the running FatRat").arg(li.file).arg(li.port);
			return p;
		}
	}
	p.explanation << tried;

	// ---- 3. start a server of our own
	if (o.mode == PlanOptions::Local)
	{
		p.error = QObject::tr("No running FatRat found through its settings.\n%1\nStart FatRat and turn on its web interface (Settings, Web interface), "
			"or use --mode socket to start a separate server, or --url.").arg(tried.join('\n'));
		return p;
	}
	if (o.noStart)
	{
		p.error = QObject::tr("Nothing to connect to, and --no-start forbids starting a server.\n%1").arg(tried.join('\n'));
		return p;
	}
	p.kind = Plan::StartPrivate;
	p.endpoint.kind = Endpoint::Unix;
	p.endpoint.socketPath = sock;
	p.description = QObject::tr("private server on a unix socket");
	p.explanation << QObject::tr("Starting a FatRat server of our own on the unix socket %1 (it uses your normal FatRat settings and transfers)").arg(sock);
	return p;
}

}
