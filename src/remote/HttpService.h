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
#include <ctime>
#include <openssl/ssl.h>
#include <pion/net/HTTPResponseWriter.hpp>
#include "remote/TransferHttpService.h"

#ifndef WITH_WEBINTERFACE
#	error This file is not supposed to be included!
#endif

#include <pion/net/WebServer.hpp>

class Queue;
class Transfer;

class HttpService : public QObject
{
public:
	HttpService();
	~HttpService();
	static HttpService* instance() { return m_instance; }
	void applySettings();
	
	void setup();
	void setupAuth();
	void setupSSL();

	static void findQueue(QString queueUUID, Queue** q);
	static int findTransfer(QString transferUUID, Queue** q, Transfer** t, bool lockForWrite = false);

	static QVariant generateCertificate(QList<QVariant>&);
private:
	static HttpService* m_instance;
	pion::net::WebServer* m_server;
	pion::net::HTTPAuthPtr m_auth_ptr;
	quint16 m_port;
	QString m_strSSLPem;

	class GraphService : public pion::net::WebService
	{
		void operator()(pion::net::HTTPRequestPtr &request, pion::net::TCPConnectionPtr &tcp_conn);
	};
	class QgraphService : public pion::net::WebService
	{
		void operator()(pion::net::HTTPRequestPtr &request, pion::net::TCPConnectionPtr &tcp_conn);
	};
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
	class CaptchaService : public pion::net::WebService
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
