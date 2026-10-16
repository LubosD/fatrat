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
#include <memory>
#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
#define CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#include "httplib.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "captcha/CaptchaHttp.h"
#include "remote/TransferHttpService.h"

#ifndef WITH_WEBINTERFACE
#	error This file is not supposed to be included!
#endif

class Queue;
class Transfer;

class HttpService : public QObject
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

	void addCaptchaEvent(int id, QString url);
	bool hasCaptchaHandlers();

	static void findQueue(QString queueUUID, Queue** q);
	static int findTransfer(QString transferUUID, Queue** q, Transfer** t, bool lockForWrite = false);

private slots:
	void keepalive(); // QTimer TODO
private:
	void addCaptchaClient(WebSocketService* client);
	void removeCaptchaClient(WebSocketService* client);
	void killCaptchaClients();

	void registerRoutes();
private:
	static HttpService* m_instance;
	CaptchaHttp m_captchaHttp;

	QTimer m_timer;
	QList<WebSocketService*> m_registeredCaptchaClients;
	QMutex m_registeredCaptchaClientsMutex;

	std::unique_ptr<httplib::Server> m_server;
	std::thread m_serverThread;
	mutable std::mutex m_passwordMutex;
	QString m_strPassword;
	QString m_strSSLPem;
	bool m_bUseSSL = false;
	quint16 m_port = 0;

	class WebSocketService
	{
	public:
		typedef QPair<int,QString> Captcha;

		void pushCaptcha(const Captcha& cap) {
			{
				std::lock_guard<std::mutex> lk(m_mutex);
				m_queue.enqueue(cap);
			}
			m_cv.notify_one();
		}

		void terminate() {
			m_terminate.store(true);
			m_cv.notify_one();
		}

		void keepalive() {
			m_sendPing.store(true);
			m_cv.notify_one();
		}

		// Blocks up to timeout_ms if queue is empty. Returns drained queue.
		QQueue<Captcha> waitAndDrain(int timeout_ms) {
			std::unique_lock<std::mutex> lk(m_mutex);
			m_cv.wait_for(lk, std::chrono::milliseconds(timeout_ms),
				[this]{ return !m_queue.isEmpty() || m_terminate.load() || m_sendPing.load(); });
			return std::move(m_queue);
		}

		bool shouldTerminate() const { return m_terminate.load(); }
		bool takePing() { return m_sendPing.exchange(false); }

	private:
		std::mutex m_mutex;
		std::condition_variable m_cv;
		QQueue<Captcha> m_queue;
		std::atomic<bool> m_terminate{false};
		std::atomic<bool> m_sendPing{false};
	};

	class WriteBackImpl : public TransferHttpService::WriteBack
	{
	public:
		explicit WriteBackImpl(httplib::Response& res) : m_res(res) {}

		void write(const char* data, size_t bytes) override {
			m_buffer.append(data, static_cast<int>(bytes));
		}
		void writeFail(QString error) override {
			m_failed = true;
			m_failMsg = error;
		}
		void writeNoCopy(void* data, size_t bytes) override {
			m_buffer.append(static_cast<const char*>(data), static_cast<int>(bytes));
		}
		void send() override {
			if (m_failed) {
				m_res.status = 500;
				m_res.set_content(m_failMsg.toUtf8().constData(), "text/plain");
			} else {
				m_res.set_content(m_buffer.constData(), m_buffer.size(),
					m_contentType.empty() ? "application/octet-stream" : m_contentType.c_str());
			}
		}
		void setContentType(const char* type) override { m_contentType = type; }

	private:
		httplib::Response& m_res;
		QByteArray m_buffer;
		std::string m_contentType;
		bool m_failed = false;
		QString m_failMsg;
	};
};


#endif
