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
#include "SettingsWebForm.h"
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
#include <QDir>
#include <QBuffer>
#include <QImage>
#include <QPainter>
#include <QMultiMap>
#include <QProcess>
#include <QFile>
#include <cstdlib>
#include <sstream>
#include <locale.h>
#include <unistd.h>
#include <string.h>
#include <algorithm>
#include <iostream>

extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;
extern QSettings* g_settings;
extern QVector<EngineEntry> g_enginesDownload;

HttpService* HttpService::m_instance = 0;

#define HTTP_HEADERS "Server: FatRat/" VERSION "\r\n"

struct AuthenticationFailure {};

HttpService::HttpService()
	: m_port(0)
{
	m_instance = this;
	applySettings();
	
	SettingsItem si;
	si.lpfnCreate = SettingsWebForm::create;
	si.title = tr("Web Interface");
	si.icon = DelayedIcon(":/fatrat/webinterface.png");
	si.pfnApply = SettingsWebForm::applySettings;
	si.webSettingsScript = "/scripts/settings/webinterface.js";
	si.webSettingsIconURL = "/img/settings/webinterface.png";
	
	addSettingsPage(si);

	// XmlRpcService::registerFunction("HttpService.generateCertificate", generateCertificate, QVector<QVariant::Type>() << QVariant::String);

	// connect(&m_timer, SIGNAL(timeout()), this, SLOT(keepalive()));
	// m_timer.start(15 * 1000);

	static XmlRpcService xmlRpcService;
	addHandler("/xmlrpc", std::ref(xmlRpcService));
	addHandler("/log", std::bind(&HttpService::logService, this, std::placeholders::_1, std::placeholders::_2));
}

HttpService::~HttpService()
{
	// killCaptchaClients();

	m_instance = 0;

	if (isRunning())
	{
		m_bAbort = true;
		wait();
	}
}


void HttpService::applySettings()
{
	try
	{
		qDebug() << "HttpService::applySettings()";
		bool enable = getSettingsValue("remote/enable").toBool();
		if(enable)
		{
			if (!isRunning())
			{
				setup();
			}
			else
			{
				quint16 port = getSettingsValue("remote/port").toUInt();
				QString pemFile = getSettingsValue("remote/ssl_pem").toString();
				bool useSSL = getSettingsValue("remote/ssl").toBool();

				if (port != m_port || m_strSSLPem != pemFile || m_bUseSSL != useSSL || (useSSL && m_strSSLPem.isEmpty()))
				{
					Logger::global()->enterLogMessage("HttpService", tr("Restarting the service due to a port or SSL config change"));

					m_bAbort = true;
					wait();
					setup();
				}
				else
					setupAuth();
			}
		}
		else if(!enable && isRunning())
		{
			m_bAbort = true;
			wait();
		}
	}
	catch(const RuntimeException& e)
	{
		qDebug() << e.what();
		Logger::global()->enterLogMessage("HttpService", e.what());
	}
}

void HttpService::setupAuth()
{

}

void HttpService::setup()
{
	m_port = getSettingsValue("remote/port").toUInt();
	
	try
	{
		QByteArray port = QByteArray::number(m_port);
		m_bAbort = false;

		mg_mgr_init(&m_mgr, this);
		m_nc = mg_bind(&m_mgr, port.data(), eventHandler);

		mg_set_protocol_http_websocket(m_nc);
		mg_enable_multithreading(m_nc);

		memset(&m_serveOpts, 0, sizeof(m_serveOpts));
		m_serveOpts.document_root = DATA_LOCATION "/data/remote";

		setupSSL();

		/*
		m_server->add_service("/xmlrpc", new XmlRpcService);
		m_server->add_service("/subclass", new SubclassService);
		m_server->add_service("/log", new LogService);
		m_server->add_service("/browse", new TransferTreeBrowserService);
		m_server->add_service("/", new pion::plugins::FileService);
		m_server->set_service_option("/", "directory", DATA_LOCATION "/data/remote");
		m_server->set_service_option("/", "file", DATA_LOCATION "/data/remote/index.html");
		m_server->add_service("/copyrights", new pion::plugins::FileService);
		m_server->set_service_option("/copyrights", "file", DATA_LOCATION "/README");
		m_server->add_service("/download", new TransferDownloadService);
		m_server->add_service("/captcha", new CaptchaService);
		*/

		start();
		Logger::global()->enterLogMessage("HttpService", tr("Listening on port %1").arg(m_port));
		std::cout << "Listening on port " << m_port << std::endl;
	}
	catch(const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		Logger::global()->enterLogMessage("HttpService", tr("Failed to start: %1").arg(e.what()));
	}
}

void HttpService::setupSSL()
{
	m_bUseSSL = getSettingsValue("remote/ssl").toBool();
	if (m_bUseSSL)
	{
		QString file = getSettingsValue("remote/ssl_pem").toString();
		if (file.isEmpty() || !QFile::exists(file))
		{
			Logger::global()->enterLogMessage("HttpService", tr("SSL key file not found, disabling HTTPS"));
			//m_server->set_ssl_flag(false);
			m_strSSLPem.clear();
		}
		else
		{
			Logger::global()->enterLogMessage("HttpService", tr("Loading a SSL key from %1").arg(file));
			m_strSSLPem = file;
			//m_server->set_ssl_key_file(file.toStdString());
			//m_server->set_ssl_flag(true);
		}
	}
	else
	{
		Logger::global()->enterLogMessage("HttpService", tr("Running in plain HTTP mode"));
		//m_server->set_ssl_flag(false);
		m_strSSLPem.clear();
	}
}

QString HttpService::mgstr2qstring(const struct mg_str& str)
{
	QByteArray ba(str.p, str.len);
	return QString::fromUtf8(ba);
}

void HttpService::eventHandler(struct mg_connection *nc, int ev, void *p)
{
	HttpService* This = HttpService::instance();

	switch (ev)
	{
		case MG_EV_HTTP_REQUEST:
		{
			struct http_message *msg = static_cast<struct http_message*>(p);
			QString path = mgstr2qstring(msg->uri);
			bool handled = false;

			qDebug() << "Handling" << path;

			for (auto e : This->m_handlers.keys())
			{
				if (path.startsWith(e))
				{
					try
					{
						handled = true;
						This->m_handlers.value(e)(nc, msg);
					}
					catch (const char* error)
					{
						mg_printf(nc, "HTTP/1.1 %s\r\nContent-Length: %lu\r\nContent-Type: text/plain\r\n\r\n",
								  error, strlen(error));
						mg_send(nc, error, strlen(error));
					}

					break;
				}
			}

			if (!handled)
			{
				static QMutex mutex;
				mutex.lock();
				mg_serve_http(nc, msg, This->m_serveOpts);
				mutex.unlock();
			}
			break;
		}
	}
}

void HttpService::run()
{
	while (!m_bAbort)
	{
		mg_mgr_poll(&m_mgr, 500);
	}

	mg_mgr_free(&m_mgr);
}

void HttpService::logService(struct mg_connection *nc, struct http_message *hm)
{
	QString path = mgstr2qstring(hm->uri);
	QString uuidTransfer = path.mid(5); // remove "/log/"
	QByteArray data;

	if (uuidTransfer.isEmpty())
		data = Logger::global()->logContents().toUtf8();
	else
	{
		Queue* q = 0;
		Transfer* t = 0;
		findTransfer(uuidTransfer, &q, &t);

		if (!q || !t)
			throw "404 Not Found";

		data = t->logContents().toUtf8();

		q->unlock();
		g_queuesLock.unlock();
	}

	mg_printf(nc, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/plain\r\n\r\n",
			  data.size());

	mg_send(nc, data.data(), data.size());
}

/*

void HttpService::TransferTreeBrowserService::operator()(const pion::http::request_ptr &request, const pion::tcp::connection_ptr &tcp_conn)
{
	pion::http::response_writer_ptr writer(pion::http::response_writer::create(tcp_conn, *request, boost::bind(&pion::tcp::connection::finish, tcp_conn)));
	QString uuidTransfer = QString::fromStdString(get_relative_resource(request->get_resource()));
	QString path = QString::fromStdString(request->get_query("path"));

	path = path.replace("+", " ");
	path = QUrl::fromPercentEncoding(path.toUtf8());

	if (uuidTransfer.startsWith("/"))
		uuidTransfer = uuidTransfer.mid(1);

	Queue* q = 0;
	Transfer* t = 0;

	if (path.contains("/..") || path.contains("../"))
	{
		writer->get_response().set_status_code(pion::http::types::RESPONSE_CODE_FORBIDDEN);
		writer->get_response().set_status_message(pion::http::types::RESPONSE_MESSAGE_FORBIDDEN);
		writer->send();
		return;
	}

	findTransfer(uuidTransfer, &q, &t);

	if (!q || !t)
	{
		writer->get_response().set_status_code(pion::http::types::RESPONSE_CODE_NOT_FOUND);
		writer->get_response().set_status_message(pion::http::types::RESPONSE_MESSAGE_NOT_FOUND);
		writer->send();
		return;
	}

	QDomDocument doc;
	QDomElement root, item;
	QFileInfoList infoList;
	QDir dir(t->dataPath());

	if (path.startsWith("/"))
		path = path.mid(1);

	if (!dir.cd(path))
	{
		writer->get_response().set_status_code(pion::http::types::RESPONSE_CODE_NOT_FOUND);
		writer->get_response().set_status_message(pion::http::types::RESPONSE_MESSAGE_NOT_FOUND);
		writer->send();
		return;
	}

	root = doc.createElement("root");
	doc.appendChild(root);

	infoList = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
	foreach (QFileInfo info, infoList)
	{
		item = doc.createElement("item");
		item.setAttribute("id", path+"/"+info.fileName());
		item.setAttribute("state", "closed");
		item.setAttribute("type", "folder");

		QDomElement content, name;
		content = doc.createElement("content");
		name = doc.createElement("name");

		name.appendChild(doc.createTextNode(info.fileName()));
		content.appendChild(name);
		item.appendChild(content);
		root.appendChild(item);
	}

	infoList = dir.entryInfoList(QDir::Files);
	foreach (QFileInfo info, infoList)
	{
		item = doc.createElement("item");
		item.setAttribute("id", path+"/"+info.fileName());
		item.setAttribute("type", "default");

		QDomElement content, name;
		content = doc.createElement("content");
		name = doc.createElement("name");

		name.appendChild(doc.createTextNode(info.fileName()));
		content.appendChild(name);
		item.appendChild(content);
		root.appendChild(item);
	}

	q->unlock();
	g_queuesLock.unlock();

	writer->write(doc.toString(0).toStdString());
	writer->send();
}

void HttpService::TransferDownloadService::operator()(const pion::http::request_ptr &request, const pion::tcp::connection_ptr &tcp_conn)
{
	pion::http::response_writer_ptr writer(pion::http::response_writer::create(tcp_conn, *request, boost::bind(&pion::tcp::connection::finish, tcp_conn)));
	QString transfer = QString::fromStdString(request->get_query("transfer"));
	QString path = QString::fromStdString(request->get_query("path"));

	path = path.replace("+", " ");
	path = QUrl::fromPercentEncoding(path.toUtf8());

	transfer = QUrl::fromPercentEncoding(transfer.toUtf8());

	Queue* q = 0;
	Transfer* t = 0;

	if (path.contains("/..") || path.contains("../"))
	{
		writer->get_response().set_status_code(pion::http::types::RESPONSE_CODE_FORBIDDEN);
		writer->get_response().set_status_message(pion::http::types::RESPONSE_MESSAGE_FORBIDDEN);
		writer->send();
		return;
	}

	findTransfer(transfer, &q, &t);

	if (!q || !t)
	{
		writer->get_response().set_status_code(pion::http::types::RESPONSE_CODE_NOT_FOUND);
		writer->get_response().set_status_message(pion::http::types::RESPONSE_MESSAGE_NOT_FOUND);
		writer->send();
		return;
	}

	if (!path.startsWith("/"))
		path.prepend("/");
	if (path.endsWith("/"))
		path = path.left(path.size()-1);
	path.prepend(t->dataPath(true));

	q->unlock();
	g_queuesLock.unlock();

	QString disposition;
	int last = path.lastIndexOf('/');
	if(last > 0)
	{
		disposition = path.mid(last+1);
		if(disposition.size() > 100)
			disposition.resize(100);
	}

	disposition = QString("attachment; filename=\"%1\"").arg(disposition);

	pion::plugins::DiskFile response_file;
	response_file.setFilePath(path.toStdString());
	response_file.update();

	pion::plugins::DiskFileSenderPtr sender_ptr(pion::plugins::DiskFileSender::create(response_file, request, tcp_conn, 8192));
	sender_ptr->get_writer()->get_response().add_header("Content-Disposition", disposition.toStdString());

	if (unsigned long long fileSize = response_file.getFileSize())
	{
		std::stringstream fileSizeStream;
		fileSizeStream << fileSize;
		sender_ptr->get_writer()->get_response().add_header("Content-Length", fileSizeStream.str());
	}

	sender_ptr->send();
}

void HttpService::SubclassService::operator()(const pion::http::request_ptr &request, const pion::tcp::connection_ptr &tcp_conn)
{
	pion::http::response_writer_ptr writer = pion::http::response_writer::create(tcp_conn, *request, boost::bind(&pion::tcp::connection::finish, tcp_conn));
	HttpService::WriteBackImpl wb = HttpService::WriteBackImpl(writer);
	QString transfer = QString::fromStdString(request->get_query("transfer"));
	QString method = QString::fromStdString(get_relative_resource(request->get_resource()));

	Queue* q = 0;
	Transfer* t = 0;

	findTransfer(transfer, &q, &t);

	if (!q || !t || !dynamic_cast<TransferHttpService*>(t))
	{
		if (t)
		{
			q->unlock();
			g_queuesLock.unlock();
		}

		writer->get_response().set_status_code(pion::http::types::RESPONSE_CODE_NOT_FOUND);
		writer->get_response().set_status_message(pion::http::types::RESPONSE_MESSAGE_NOT_FOUND);
		writer->send();
		return;
	}

	QMultiMap<QString,QString> map;
	pion::ihash_multimap params = request->get_queries();
	for (pion::ihash_multimap::iterator it = params.begin(); it != params.end(); it++)
	{
		map.insert(QString::fromStdString(it->first), QString::fromStdString(it->second));
	}

	TransferHttpService* s = dynamic_cast<TransferHttpService*>(t);
	s->process(method, map, &wb);
	//writer->send();

	q->unlock();
	g_queuesLock.unlock();
}

HttpService::WriteBackImpl::WriteBackImpl(pion::http::response_writer_ptr& writer)
	: m_writer(writer)
{

}

void HttpService::WriteBackImpl::writeNoCopy(void* data, size_t bytes)
{
	m_writer->write_no_copy(data, bytes);
}

void HttpService::WriteBackImpl::send()
{
	m_writer->send();
}

void HttpService::WriteBackImpl::write(const char* data, size_t bytes)
{
	m_writer->write(data, bytes);
}

void HttpService::WriteBackImpl::setContentType(const char* type)
{
	m_writer->get_response().add_header("Content-Type", type);
}

void HttpService::WriteBackImpl::writeFail(QString error)
{
	m_writer->get_response().set_status_code(pion::http::types::RESPONSE_CODE_NOT_FOUND);
	m_writer->get_response().set_status_message(pion::http::types::RESPONSE_MESSAGE_NOT_FOUND);
}

void HttpService::CaptchaHttpResponseWriter::handle_write(const boost::system::error_code &write_error, std::size_t bytes_written)
{
	if (!bytes_written)
	{
		// TODO: handle errors
		HttpService::instance()->removeCaptchaClient(client);
		send_final_chunk();
		delete client;
	}

	pion::http::response_writer::handle_write(write_error, bytes_written);
}

void HttpService::CaptchaService::operator()(const pion::http::request_ptr &request, const pion::tcp::connection_ptr &tcp_conn)
{
	if (request->has_query("id"))
	{
		QString id = QString::fromStdString(request->get_query("id"));
		QString solution = QString::fromStdString(request->get_query("solution"));

		int iid = id.toInt();
		HttpService::instance()->m_captchaHttp.captchaEntered(iid, solution);

		pion::http::response_writer_ptr writer = pion::http::response_writer::create(tcp_conn, *request, boost::bind(&pion::tcp::connection::finish, tcp_conn));
		writer->send();
	}
	else
	{
		// PROCEDURE
		// Add an active connection

		// ON EVENT:
		// add event to queue
		// lock the mutex - exit if locked
		// send outstanding events
		// unlock the mutex in the finished handler
		// remove connection if handleWrite receives an error

		RegisteredClient* client = new RegisteredClient;

		client->writer = CaptchaHttpResponseWriter::create(client, tcp_conn, *request,
								   boost::bind(&pion::tcp::connection::finish, tcp_conn));
		HttpService::instance()->addCaptchaClient(client);

		client->writer->get_response().add_header("Content-Type", "text/event-stream");
		client->writer->get_response().add_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
		client->writer->get_response().add_header("Expires", "Tue, 03 Jul 2001 06:00:00 GMT");
		client->writer->get_response().add_header("Pragma", "no-cache");
	}
}

void HttpService::RegisteredClient::finished()
{
	writeInProgressLock.unlock();
	pushMore();
}

void HttpService::RegisteredClient::pushMore()
{
	if (!writeInProgressLock.tryLock())
		return;

	captchaQueueLock.lock();

	QByteArray buf;
	while(!captchaQueue.isEmpty())
	{
		Captcha cap = captchaQueue.dequeue();
		buf.append("data: ").append(QString::number(cap.first)).append(",").append(cap.second)
				.append("\r\nid: ").append(QString::number(cap.first)).append("\r\n\r\n");
	}

	captchaQueueLock.unlock();

	if (!buf.isEmpty())
	{
		writer->clear();

		writer->write(buf.data(), buf.length());
		writer->send_chunk(boost::bind(&HttpService::RegisteredClient::finished, this));
	}
	else
		writeInProgressLock.unlock();
}

void HttpService::RegisteredClient::keepalive()
{
	if (!writeInProgressLock.tryLock())
		return;

	writer->clear();
	writer->write(": keepalive\r\n\r\n");
	writer->send_chunk(boost::bind(&HttpService::RegisteredClient::finished, this));
}

void HttpService::addCaptchaEvent(int id, QString url)
{
	QMutexLocker l(&m_registeredCaptchaClientsMutex);

	foreach(RegisteredClient* cl, m_registeredCaptchaClients)
	{
		cl->captchaQueueLock.lock();
		cl->captchaQueue << RegisteredClient::Captcha(id, url);
		cl->captchaQueueLock.unlock();
		cl->pushMore();
	}
}

void HttpService::addCaptchaClient(RegisteredClient* client)
{
	QMutexLocker l(&m_registeredCaptchaClientsMutex);
	m_registeredCaptchaClients << client;
}

void HttpService::removeCaptchaClient(RegisteredClient* client)
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

	foreach(RegisteredClient* cl, m_registeredCaptchaClients)
		cl->keepalive();
}

void HttpService::RegisteredClient::terminate()
{
	writer->get_connection()->finish();
}
*/


void HttpService::addCaptchaEvent(int id, QString url)
{

}

bool HttpService::hasCaptchaHandlers()
{
	return false;
}

int HttpService::findTransfer(QString transferUUID, Queue** q, Transfer** t, bool lockForWrite)
{
	*q = 0;
	*t = 0;

	g_queuesLock.lockForRead();
	for(int i=0;i<g_queues.size();i++)
	{
		Queue* c = g_queues[i];
		if (lockForWrite)
			c->lockW();
		else
			c->lock();

		for(int j=0;j<c->size();j++)
		{
			if(c->at(j)->uuid() == transferUUID)
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
	for(int i=0;i<g_queues.size();i++)
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
