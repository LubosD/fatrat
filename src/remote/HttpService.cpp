/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2010 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 3 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#include "config.h"

#include "HttpService.h"
#include "Settings.h"
#include "RuntimeException.h"
#ifdef WITH_GUI
#	include "SettingsWebForm.h"
#endif
#include "SpeedGraph.h"
#include "Transfer.h"
#include "Queue.h"
#include "fatrat.h"
#include "Logger.h"
#include "TransferFactory.h"
#include "dbus/DbusImpl.h"
#include "remote/XmlRpcService.h"

#include <QSettings>
#include <QtDebug>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QDir>
#include <QBuffer>
#include <QImage>
#include <QPainter>
#include <QMultiMap>
#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMimeDatabase>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <thread>
#include <locale.h>
#include <unistd.h>
#include <string.h>
#include <algorithm>

extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;
extern QSettings* g_settings;
extern QVector<EngineEntry> g_enginesDownload;

HttpService* HttpService::m_instance = nullptr;

HttpService::HttpService()
	: m_port(0)
{
	m_instance = this;
	applySettings();

	SettingsItem si;
#ifdef WITH_GUI
	si.lpfnCreate = SettingsWebForm::create;
#endif
	si.title = tr("Web Interface");
	si.icon = DelayedIcon(":/fatrat/webinterface.png");
	si.pfnApply = [] { HttpService::instance()->applySettings(); };
	si.webSettingsScript = "/scripts/settings/webinterface.js";
	si.webSettingsIconURL = "/img/settings/webinterface.png";

	addSettingsPage(si);

	XmlRpcService::registerFunction("HttpService.generateCertificate", generateCertificate,
		QVector<QVariant::Type>() << QVariant::String);

	connect(&m_timer, SIGNAL(timeout()), this, SLOT(keepalive()));
	m_timer.start(15 * 1000);
}

HttpService::~HttpService()
{
	killCaptchaClients();

	m_instance = nullptr;
	if (m_server) {
		m_server->stop();
		if (m_serverThread.joinable())
			m_serverThread.join();
	}
}

void HttpService::killCaptchaClients()
{
	QMutexLocker l(&m_registeredCaptchaClientsMutex);

	for (WebSocketService* s : m_registeredCaptchaClients)
	{
		s->terminate();
	}
	m_registeredCaptchaClients.clear();
}

void HttpService::applySettings()
{
	try
	{
		qDebug() << "HttpService::applySettings()";
		bool enable = getSettingsValue("remote/enable").toBool();
		if (enable)
		{
			if (!m_server)
			{
				setup();
			}
			else
			{
				quint16 port = getSettingsValue("remote/port").toUInt();
				QString pemFile = getSettingsValue("remote/ssl_pem").toString();
				bool useSSL = getSettingsValue("remote/ssl").toBool();

				if (port != m_port || m_strSSLPem != pemFile || m_bUseSSL != useSSL)
				{
					Logger::global()->enterLogMessage("HttpService",
						tr("Restarting the service due to a port or SSL config change"));

					killCaptchaClients();
					m_server->stop();
					if (m_serverThread.joinable())
						m_serverThread.join();
					m_server.reset();
					setup();
				}
				else
				{
					std::lock_guard<std::mutex> lk(m_passwordMutex);
					m_strPassword = getSettingsValue("remote/password").toString();
				}
			}
		}
		else if (!enable && m_server)
		{
			killCaptchaClients();
			m_server->stop();
			if (m_serverThread.joinable())
				m_serverThread.join();
			m_server.reset();
		}
	}
	catch (const RuntimeException& e)
	{
		qDebug() << e.what();
		Logger::global()->enterLogMessage("HttpService", e.what());
	}
}

void HttpService::setup()
{
	m_port = getSettingsValue("remote/port").toUInt();
	{
		std::lock_guard<std::mutex> lk(m_passwordMutex);
		m_strPassword = getSettingsValue("remote/password").toString();
	}
	m_bUseSSL = getSettingsValue("remote/ssl").toBool();

	if (m_bUseSSL)
	{
		QString file = getSettingsValue("remote/ssl_pem").toString();
		if (file.isEmpty() || !QFile::exists(file))
		{
			Logger::global()->enterLogMessage("HttpService",
				tr("SSL key file not found, disabling HTTPS"));
			m_server = std::make_unique<httplib::Server>();
			m_strSSLPem.clear();
		}
		else
		{
			Logger::global()->enterLogMessage("HttpService",
				tr("Loading a SSL key from %1").arg(file));
			m_strSSLPem = file;
			auto s = std::make_unique<httplib::SSLServer>(
				file.toStdString().c_str(),
				file.toStdString().c_str()
			);
			m_server = std::move(s);
		}
	}
	else
	{
		Logger::global()->enterLogMessage("HttpService", tr("Running in plain HTTP mode"));
		m_server = std::make_unique<httplib::Server>();
	}

	registerRoutes();

	Logger::global()->enterLogMessage("HttpService",
		tr("Listening on port %1").arg(m_port));

	m_serverThread = std::thread([this]() {
		m_server->listen("0.0.0.0", static_cast<int>(m_port));
	});
}

void HttpService::registerRoutes()
{
	// Auth pre-routing hook
	m_server->set_pre_routing_handler([this](const httplib::Request& req, httplib::Response& res) {
		QString password;
		{
			std::lock_guard<std::mutex> lk(m_passwordMutex);
			password = m_strPassword;
		}
		if (password.isEmpty())
			return httplib::Server::HandlerResponse::Unhandled;

		std::string authHeader = req.get_header_value("Authorization");
		bool ok = false;
		if (authHeader.size() > 6 && authHeader.substr(0, 6) == "Basic ") {
			QByteArray decoded = QByteArray::fromBase64(
				QByteArray(authHeader.c_str() + 6, static_cast<int>(authHeader.size() - 6)));
			int colon = decoded.indexOf(':');
			QString pw = (colon >= 0)
				? QString::fromUtf8(decoded.mid(colon + 1))
				: QString::fromUtf8(decoded);
			ok = (pw == password);
		}
		if (!ok) {
			res.status = 401;
			res.set_header("WWW-Authenticate", "Basic realm=\"FatRat\"");
			res.set_content("Unauthorized", "text/plain");
			return httplib::Server::HandlerResponse::Handled;
		}
		return httplib::Server::HandlerResponse::Unhandled;
	});

	// /xmlrpc
	m_server->Post("/xmlrpc", [this](const httplib::Request& req, httplib::Response& res) {
		m_xmlRpc.handleRequest(req, res);
	});

	// /log and /log/<transfer-uuid>
	m_server->Get(R"(/log(.*))", [](const httplib::Request& req, httplib::Response& res) {
		QString uuidTransfer;
		if (req.matches.size() > 1) {
			std::string suffix = req.matches[1].str();
			if (!suffix.empty() && suffix[0] == '/')
				suffix = suffix.substr(1);
			uuidTransfer = QUrl::fromPercentEncoding(
				QByteArray::fromStdString(suffix));
		}

		QString data;
		if (uuidTransfer.isEmpty()) {
			data = Logger::global()->logContents();
		} else {
			Queue* q = nullptr; Transfer* t = nullptr;
			findTransfer(uuidTransfer, &q, &t);
			if (!q || !t) {
				res.status = 404;
				res.set_content("Queue/Transfer not found", "text/plain");
				return;
			}
			data = t->logContents();
			q->unlock();
			g_queuesLock.unlock();
		}

		QByteArray ba = data.toUtf8();
		res.set_content(ba.constData(), ba.size(), "text/plain");
	});

	// /subclass/<method>?transfer=<uuid>
	m_server->Get(R"(/subclass/(.+))", [this](const httplib::Request& req, httplib::Response& res) {
		QString method = QString::fromStdString(req.matches[1].str());
		QString transfer = QString::fromStdString(req.get_param_value("transfer"));

		Queue* q = nullptr; Transfer* t = nullptr;
		findTransfer(transfer, &q, &t);

		if (!q || !t) {
			res.status = 404;
			res.set_content("Transfer UUID not found", "text/plain");
			return;
		}

		TransferHttpService* s = dynamic_cast<TransferHttpService*>(t);
		if (!s) {
			q->unlock(); g_queuesLock.unlock();
			res.status = 404;
			res.set_content("Transfer does not support HTTP service", "text/plain");
			return;
		}

		QMultiMap<QString,QString> map;
		for (const auto& kv : req.params)
			map.insert(QString::fromStdString(kv.first), QString::fromStdString(kv.second));

		WriteBackImpl wb(res);
		s->process(method, map, &wb);

		q->unlock();
		g_queuesLock.unlock();
		wb.send();
	});

	// /browse[/<transfer-uuid>]?path=<path>
	m_server->Get(R"(/browse(?:/(.*))?)", [](const httplib::Request& req, httplib::Response& res) {
		QString uuidTransfer;
		if (req.matches.size() > 1 && req.matches[1].matched)
			uuidTransfer = QUrl::fromPercentEncoding(
				QByteArray::fromStdString(req.matches[1].str()));

		if (uuidTransfer.startsWith("/"))
			uuidTransfer = uuidTransfer.mid(1);

		QString path = QString::fromStdString(req.get_param_value("path"));
		if (path.contains("/..") || path.contains("../")) {
			res.status = 403; res.set_content(".. in path", "text/plain"); return;
		}

		Queue* q = nullptr; Transfer* t = nullptr;
		findTransfer(uuidTransfer, &q, &t);
		if (!q || !t) {
			res.status = 404; res.set_content("Transfer UUID not found", "text/plain"); return;
		}

		QDomDocument doc;
		QDomElement root;
		QDir dir(t->dataPath());

		if (path.startsWith("/")) path = path.mid(1);
		if (!dir.cd(path)) {
			q->unlock(); g_queuesLock.unlock();
			res.status = 404; res.set_content("Not found", "text/plain"); return;
		}

		root = doc.createElement("root");
		doc.appendChild(root);

		for (const QFileInfo& info : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
			QDomElement item = doc.createElement("item");
			item.setAttribute("id", path+"/"+info.fileName());
			item.setAttribute("state", "closed");
			item.setAttribute("type", "folder");
			QDomElement content = doc.createElement("content");
			QDomElement name = doc.createElement("name");
			name.appendChild(doc.createTextNode(info.fileName()));
			content.appendChild(name);
			item.appendChild(content);
			root.appendChild(item);
		}
		for (const QFileInfo& info : dir.entryInfoList(QDir::Files)) {
			QDomElement item = doc.createElement("item");
			item.setAttribute("id", path+"/"+info.fileName());
			item.setAttribute("type", "default");
			QDomElement content = doc.createElement("content");
			QDomElement name = doc.createElement("name");
			name.appendChild(doc.createTextNode(info.fileName()));
			content.appendChild(name);
			item.appendChild(content);
			root.appendChild(item);
		}

		q->unlock(); g_queuesLock.unlock();

		QByteArray ba = doc.toString(0).toUtf8();
		res.set_content(ba.constData(), ba.size(), "text/xml");
	});

	// /download?transfer=<uuid>&path=<path>
	m_server->Get("/download", [](const httplib::Request& req, httplib::Response& res) {
		QString transfer = QString::fromStdString(req.get_param_value("transfer"));
		QString path     = QString::fromStdString(req.get_param_value("path"));

		if (path.contains("/..") || path.contains("../")) {
			res.status = 403; res.set_content(".. in path", "text/plain"); return;
		}

		Queue* q = nullptr; Transfer* t = nullptr;
		findTransfer(transfer, &q, &t);
		if (!q || !t) {
			res.status = 404; res.set_content("Transfer UUID not found", "text/plain"); return;
		}

		if (!path.startsWith("/")) path.prepend("/");
		if (path.endsWith("/")) path.chop(1);
		path.prepend(t->dataPath(true));

		q->unlock(); g_queuesLock.unlock();

		QString disposition;
		int last = path.lastIndexOf('/');
		if (last >= 0)
			disposition = path.mid(last + 1).left(100);

		res.set_header("Content-Disposition",
			QString("attachment; filename=\"%1\"").arg(disposition).toStdString());

		auto filePtr = std::make_shared<QFile>(path);
		if (!filePtr->open(QIODevice::ReadOnly)) {
			res.status = 404;
			res.set_content("File not found", "text/plain");
			return;
		}
		qint64 fileSize = filePtr->size();

		res.set_content_provider(
			static_cast<size_t>(fileSize),
			"application/octet-stream",
			[filePtr](size_t offset, size_t length, httplib::DataSink& sink) {
				if (!filePtr->seek(static_cast<qint64>(offset)))
					return false;
				QByteArray chunk = filePtr->read(static_cast<qint64>(length));
				return sink.write(chunk.constData(), static_cast<size_t>(chunk.size()));
			}
		);
	});

	// /copyrights
	m_server->Get("/copyrights", [](const httplib::Request&, httplib::Response& res) {
		QFile f(DATA_LOCATION "/README");
		if (!f.open(QIODevice::ReadOnly)) {
			res.status = 404; res.set_content("Not Found", "text/plain"); return;
		}
		QByteArray data = f.readAll();
		res.set_content(data.constData(), data.size(), "text/plain");
	});

	// /captcha (POST)
	m_server->Post("/captcha", [this](const httplib::Request& req, httplib::Response& res) {
		std::string id_str  = req.get_param_value("id");
		std::string sol_str = req.get_param_value("solution");
		if (id_str.empty() || sol_str.empty()) {
			res.status = 400; res.set_content("id or solution missing", "text/plain"); return;
		}
		int id = std::stoi(id_str);
		m_captchaHttp.captchaEntered(id, QString::fromStdString(sol_str));
		res.status = 204;
	});

	// /websocket
	m_server->WebSocket("/websocket",
		[this](const httplib::Request&, httplib::ws::WebSocket& ws) {
			auto svc = std::make_shared<WebSocketService>();
			addCaptchaClient(svc.get());

			// Drive the read loop in a background thread so that incoming
			// control frames (close, ping) are processed and the connection
			// is detected as closed promptly.
			std::thread reader([&ws, &svc]() {
				std::string msg;
				while (ws.read(msg) != httplib::ws::ReadResult::Fail) {
					// We don't expect inbound text messages from the browser
					// client, so just discard them.
				}
				// read() returned Fail — connection closed by the client.
				svc->terminate();
			});

			bool running = true;
			while (running && !svc->shouldTerminate()) {
				QQueue<WebSocketService::Captcha> queue = svc->waitAndDrain(500);

				if (svc->shouldTerminate()) break;

				if (svc->takePing()) {
					if (!ws.send("{\"type\": \"ping\"}")) break;
				}

				while (!queue.isEmpty()) {
					if (svc->shouldTerminate()) { running = false; break; }
					QJsonObject notif;
					auto cap = queue.dequeue();
					notif.insert("type", "captcha");
					notif.insert("id", cap.first);
					notif.insert("url", cap.second);
					QByteArray ba = QJsonDocument(notif).toJson(QJsonDocument::Compact);
					if (!ws.send(ba.toStdString())) { running = false; break; }
				}
			}

			// Close the WebSocket so the reader thread's ws.read() unblocks.
			ws.close();
			reader.join();

			removeCaptchaClient(svc.get());
		}
	);

	// Static files (must be last)
	m_server->set_mount_point("/", DATA_LOCATION "/data/remote/");
}

int HttpService::findTransfer(QString transferUUID, Queue** q, Transfer** t, bool lockForWrite)
{
	*q = 0;
	*t = 0;

	g_queuesLock.lockForRead();
	for (int i = 0; i < g_queues.size(); i++)
	{
		Queue* c = g_queues[i];
		if (lockForWrite)
			c->lockW();
		else
			c->lock();

		for (int j = 0; j < c->size(); j++)
		{
			if (c->at(j)->uuid() == transferUUID)
			{
				*q = c;
				*t = c->at(j);
				return j;
			}
		}

		c->unlock();
	}

	g_queuesLock.unlock();
	return -1;
}

void HttpService::findQueue(QString queueUUID, Queue** q)
{
	*q = 0;

	QReadLocker r(&g_queuesLock);
	for (int i = 0; i < g_queues.size(); i++)
	{
		Queue* c = g_queues[i];

		if (c->uuid() == queueUUID)
		{
			*q = c;
			return;
		}
	}
}

QVariant HttpService::generateCertificate(QList<QVariant>& args)
{
	QString hostname = args[0].toString();
	const char* script = DATA_LOCATION "/data/genssl.sh";
	const char* config = DATA_LOCATION "/data/genssl.cnf";
	const char* pemfile = "/tmp/fatrat-webui.pem";

	QProcess prc;

	qDebug() << "Starting: " << script << " " << config;
	prc.start(script, QStringList() << config << hostname);
	prc.waitForFinished();

	if (prc.exitCode() != 0)
		throw XmlRpcService::XmlRpcError(501, tr("Failed to generate a certificate, please ensure you have 'openssl' and 'sed' installed."));

	QFile file(pemfile);
	if (!file.open(QIODevice::ReadOnly))
		throw XmlRpcService::XmlRpcError(501, tr("Failed to generate a certificate, please ensure you have 'openssl' and 'sed' installed."));

	QByteArray data = file.readAll();
	QDir::home().mkpath(USER_PROFILE_PATH "/data");

	QString path = QDir::homePath() + QLatin1String(USER_PROFILE_PATH) + "/data/fatrat-webui.pem";
	QFile out(path);

	if (!out.open(QIODevice::WriteOnly))
		throw XmlRpcService::XmlRpcError(502, tr("Failed to open %1 for writing.").arg(path));

	out.write(data);

	out.setPermissions(QFile::ReadOwner|QFile::WriteOwner);
	out.close();
	file.remove();

	return path;
}

void HttpService::addCaptchaEvent(int id, QString url)
{
	QMutexLocker l(&m_registeredCaptchaClientsMutex);

	for (WebSocketService* ws : m_registeredCaptchaClients)
	{
		ws->pushCaptcha(WebSocketService::Captcha(id, url));
	}
}

void HttpService::addCaptchaClient(WebSocketService* client)
{
	QMutexLocker l(&m_registeredCaptchaClientsMutex);
	m_registeredCaptchaClients << client;
}

void HttpService::removeCaptchaClient(WebSocketService* client)
{
	QMutexLocker l(&m_registeredCaptchaClientsMutex);
	m_registeredCaptchaClients.removeAll(client);
}

bool HttpService::hasCaptchaHandlers()
{
	return !m_registeredCaptchaClients.isEmpty();
}

void HttpService::keepalive()
{
	QMutexLocker l(&m_registeredCaptchaClientsMutex);

	for (WebSocketService* cl : m_registeredCaptchaClients)
		cl->keepalive();
}
