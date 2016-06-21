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
#include <QRegExp>
#include <ctime>
#include <functional>
#include <memory>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/WebSocket.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include "captcha/CaptchaHttp.h"
#include "remote/TransferHttpService.h"
#include "XmlRpcService.h"

using namespace Poco::Net;

#ifndef WITH_WEBINTERFACE
#	error This file is not supposed to be included!
#endif

class Queue;
class Transfer;

class HttpService : public QObject, public HTTPRequestHandlerFactory
{
Q_OBJECT
private:
	class WebSocketService;
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

	virtual HTTPRequestHandler* createRequestHandler(const HTTPServerRequest& request) override;
private slots:
	void keepalive(); // QTimer TODO
private:
	void addCaptchaClient(WebSocketService* client);
	void removeCaptchaClient(WebSocketService* client);
	void killCaptchaClients();

	typedef std::function<HTTPRequestHandler*()> handler_t;
	void addHandler(const QRegExp& path, handler_t handler) { m_handlers << QPair<QRegExp, handler_t>(path, handler); }

	void logService(HTTPServerRequest &req, HTTPServerResponse &resp);
private:
	static Poco::SharedPtr<HttpService> m_instance;
	CaptchaHttp m_captchaHttp;
	quint16 m_port;
	QString m_strSSLPem;
	bool m_bUseSSL;

	QTimer m_timer;
	QList<WebSocketService*> m_registeredCaptchaClients;
	QMutex m_registeredCaptchaClientsMutex;

	HTTPServer* m_server;
	std::unique_ptr<ServerSocket> m_socket;

	QList<QPair<QRegExp, handler_t>> m_handlers;
	QMutex m_handlersMutex;
	XmlRpcService m_xmlRpc;

	class LogService : public AuthenticatedRequestHandler
	{
	public:
		LogService(const QString& mapping);
		virtual void run() override;
	private:
		QString m_mapping;
	};

	class TransferTreeBrowserService : public AuthenticatedRequestHandler
	{
	public:
		TransferTreeBrowserService(const QString& mapping);
		virtual void run() override;
	private:
		QString m_mapping;
	};
	class TransferDownloadService : public AuthenticatedRequestHandler
	{
	public:
		TransferDownloadService(const QString& mapping);
		virtual void run() override;
	private:
		QString m_mapping;
	};
	class SubclassService : public AuthenticatedRequestHandler
	{
	public:
		SubclassService(const QString& mapping);
		virtual void run() override;
	private:
		QString m_mapping;
	};
	class CaptchaService : public AuthenticatedRequestHandler
	{
	public:
		virtual void run() override;
	};
	class WebSocketService : public AuthenticatedRequestHandler
	{
	public:
		WebSocketService();
		~WebSocketService();
		virtual void run() override;

		typedef QPair<int,QString> Captcha;

		// Enqueue a captcha and have it delivered
		void pushCaptcha(const Captcha& cap);

		void terminate() { wakeup(-1); }
		void keepalive() { wakeup(1); }
	private:
		// Send captchas in queue to client
		void pushMore(WebSocket& ws);

		// Wakeup the loop
		void wakeup(int v);
	private:
		int m_pipe[2];
		QMutex writeInProgressLock;
		QQueue<Captcha> captchaQueue;
		QMutex captchaQueueLock;
	};

	class WriteBackImpl : public TransferHttpService::WriteBack
	{
	public:
		WriteBackImpl(Poco::Net::HTTPServerResponse& resp);
		void write(const char* data, size_t bytes) override;
		void writeFail(QString error) override;
		void writeNoCopy(void* data, size_t bytes) override;
		void send() override;
		void setContentType(const char* type) override;
	private:
		void sendHeaders();
	private:
		Poco::Net::HTTPServerResponse& m_response;
		std::ostream* m_ostream = nullptr;
	};
};


#endif
