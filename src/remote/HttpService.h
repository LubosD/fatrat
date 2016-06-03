/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2011 Lubos Dolezel <lubos a dolezel.info>

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

#ifndef HTTPSERVICE_H
#define HTTPSERVICE_H
#include "config.h"
#include <QThread>
#include <QMap>
#include <QByteArray>
#include <QVariantMap>
#include <QFile>
#include <QMutex>
#include <QList>
#include <QQueue>
#include <QTimer>
#include <ctime>
#include <functional>
#include "mongoose.h"
#include "captcha/CaptchaHttp.h"
#include "remote/TransferHttpService.h"

#ifndef WITH_WEBINTERFACE
#	error This file is not supposed to be included!
#endif

class Queue;
class Transfer;

class HttpService : public QThread
{
Q_OBJECT
private:
	struct RegisteredClient;
public:
	HttpService();
	~HttpService();
	static HttpService* instance() { return m_instance; }
	void applySettings();
	
	void setup();
	void setupAuth();
	void setupSSL();

	void addCaptchaEvent(int id, QString url);
	bool hasCaptchaHandlers();

	static void findQueue(QString queueUUID, Queue** q);
	static int findTransfer(QString transferUUID, Queue** q, Transfer** t, bool lockForWrite = false);

	static QVariant generateCertificate(QList<QVariant>&);
	static QString mgstr2qstring(const struct mg_str& str);
//private slots:
	// void keepalive(); // QTimer TODO
protected:
	virtual void run() override;
private:
	static void eventHandler(struct mg_connection *nc, int ev, void *p);

	typedef std::function<void(struct mg_connection *nc, struct http_message *hm)> handler_t;
	void addHandler(const QString& path, handler_t handler) { m_handlers[path] = handler; }

	void logService(struct mg_connection *nc, struct http_message *hm);

	/*void addCaptchaClient(RegisteredClient* client);
	void removeCaptchaClient(RegisteredClient* client);
	void killCaptchaClients();*/
private:
	static HttpService* m_instance;
	CaptchaHttp m_captchaHttp;
	quint16 m_port;
	QString m_strSSLPem;
	bool m_bUseSSL;
	bool m_bAbort;

	// Mongoose stuff
	struct mg_mgr m_mgr;
	struct mg_connection* m_nc;
	struct mg_serve_http_opts m_serveOpts;

	QMap<QString, handler_t> m_handlers;

	QTimer m_timer;
	QList<RegisteredClient*> m_registeredCaptchaClients;
	QMutex m_registeredCaptchaClientsMutex;

	/*
	class TransferTreeBrowserService : public pion::http::plugin_service
	{
		void operator()(const pion::http::request_ptr &request, const pion::tcp::connection_ptr &tcp_conn) override;
	};
	class TransferDownloadService : public pion::http::plugin_service
	{
		void operator()(const pion::http::request_ptr &request, const pion::tcp::connection_ptr &tcp_conn) override;
	};
	class SubclassService : public pion::http::plugin_service
	{
	public:
		void operator()(const pion::http::request_ptr &request, const pion::tcp::connection_ptr &tcp_conn) override;
	};

	class CaptchaHttpResponseWriter;
	class CaptchaService : public pion::http::plugin_service
	{
	public:
		void operator()(const pion::http::request_ptr &request, const pion::tcp::connection_ptr &tcp_conn);
	};
	struct RegisteredClient
	{
		typedef QPair<int,QString> Captcha;

		boost::shared_ptr<CaptchaHttpResponseWriter> writer;
		QMutex writeInProgressLock;
		QQueue<Captcha> captchaQueue;
		QMutex captchaQueueLock;

		void keepalive();
		void pushMore();
		void finished();
		void terminate();
	};

	class CaptchaHttpResponseWriter : public pion::http::response_writer
	{
	public:
		CaptchaHttpResponseWriter(HttpService::RegisteredClient* cl, const pion::tcp::connection_ptr &tcp_conn, const pion::http::request& request, finished_handler_t handler = finished_handler_t())
					      : pion::http::response_writer(tcp_conn, request, handler), client(cl)
		{

		}

		static inline boost::shared_ptr<CaptchaHttpResponseWriter> create(HttpService::RegisteredClient* cl, const pion::tcp::connection_ptr &tcp_conn, const pion::http::request& request, finished_handler_t handler = finished_handler_t())
		{
			return boost::shared_ptr<CaptchaHttpResponseWriter>(new CaptchaHttpResponseWriter(cl, tcp_conn, request, handler));
		}
		virtual void handle_write(const boost::system::error_code &write_error, std::size_t bytes_written);

		HttpService::RegisteredClient* client;
	};

	class WriteBackImpl : public TransferHttpService::WriteBack
	{
	public:
		WriteBackImpl(pion::http::response_writer_ptr& writer);
		void write(const char* data, size_t bytes);
		void writeFail(QString error);
		void writeNoCopy(void* data, size_t bytes);
		void send();
		void setContentType(const char* type);
	private:
		pion::http::response_writer_ptr m_writer;
	};
	*/
};


#endif
