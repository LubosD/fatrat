/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2011 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

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
#include "openssl_debian_workaround.h"
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
#include <openssl/ssl.h>
#include <pion/net/HTTPResponseWriter.hpp>
#include "captcha/CaptchaHttp.h"
#include "remote/TransferHttpService.h"

#ifndef WITH_WEBINTERFACE
#	error This file is not supposed to be included!
#endif

#include <pion/net/WebServer.hpp>

class Queue;
class Transfer;

class HttpService : public QObject
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
private slots:
	void keepalive(); // QTimer TODO
private:
	void addCaptchaClient(RegisteredClient* client);
	void removeCaptchaClient(RegisteredClient* client);
	void killCaptchaClients();
private:
	static HttpService* m_instance;
	pion::net::WebServer* m_server;
	pion::net::HTTPAuthPtr m_auth_ptr;
	CaptchaHttp m_captchaHttp;
	quint16 m_port;
	QString m_strSSLPem;

	QTimer m_timer;
	QList<RegisteredClient*> m_registeredCaptchaClients;
	QMutex m_registeredCaptchaClientsMutex;

	class LogService : public pion::net::WebService
	{
		void operator()(pion::net::HTTPRequestPtr &request, pion::net::TCPConnectionPtr &tcp_conn);
	};
	class TransferTreeBrowserService : public pion::net::WebService
	{
		void operator()(pion::net::HTTPRequestPtr &request, pion::net::TCPConnectionPtr &tcp_conn);
	};
	class TransferDownloadService : public pion::net::WebService
	{
		void operator()(pion::net::HTTPRequestPtr &request, pion::net::TCPConnectionPtr &tcp_conn);
	};
	class SubclassService : public pion::net::WebService
	{
	public:
		void operator()(pion::net::HTTPRequestPtr &request, pion::net::TCPConnectionPtr &tcp_conn);
	};
	/*class CaptchaService : public pion::net::WebService
	{
	public:
		void operator()(pion::net::HTTPRequestPtr &request, pion::net::TCPConnectionPtr &tcp_conn);
	private:
		class CapServCap
		{
		public:
			void operator()(const boost::system::error_code& error, std::size_t bytes_transferred);
			void readDone();

			pion::net::HTTPResponseWriterPtr writer;
			std::string key1, key2;
			char sig[8];
			int inbuf;
			pion::net::TCPConnectionPtr tcp_conn;
		} m_cap;
	};*/

	class CaptchaHttpResponseWriter;
	class CaptchaService : public pion::net::WebService
	{
	public:
		void operator()(pion::net::HTTPRequestPtr &request, pion::net::TCPConnectionPtr &tcp_conn);
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

	class CaptchaHttpResponseWriter : public pion::net::HTTPResponseWriter
	{
	public:
		CaptchaHttpResponseWriter(HttpService::RegisteredClient* cl, pion::net::TCPConnectionPtr &tcp_conn, const pion::net::HTTPRequest& request, FinishedHandler handler = FinishedHandler())
					      : pion::net::HTTPResponseWriter(tcp_conn, request, handler), client(cl)
		{

		}

		static inline boost::shared_ptr<CaptchaHttpResponseWriter> create(HttpService::RegisteredClient* cl, pion::net::TCPConnectionPtr &tcp_conn, const pion::net::HTTPRequest& request, FinishedHandler handler = FinishedHandler())
		{
			return boost::shared_ptr<CaptchaHttpResponseWriter>(new CaptchaHttpResponseWriter(cl, tcp_conn, request, handler));
		}
		/*virtual void prepareBuffersForSend(HTTPMessage::WriteBuffers& write_buffers) {
			m_content_length = 0;
			//if (getContentLength() > 0)
			//	m_http_response->setContentLength(getContentLength());
			m_http_response->prepareBuffersForSend(write_buffers, getTCPConnection()->getKeepAlive(),
							       sendingChunkedMessage());
		}*/
		virtual void handleWrite(const boost::system::error_code &write_error, std::size_t bytes_written);

		HttpService::RegisteredClient* client;
	};

	class WriteBackImpl : public TransferHttpService::WriteBack
	{
	public:
		WriteBackImpl(pion::net::HTTPResponseWriterPtr& writer);
		void write(const char* data, size_t bytes);
		void writeFail(QString error);
		void writeNoCopy(void* data, size_t bytes);
		void send();
		void setContentType(const char* type);
	private:
		pion::net::HTTPResponseWriterPtr m_writer;
	};
};


#endif
