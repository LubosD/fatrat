#pragma once
#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QSslError>
#include <QUrl>
#include <deque>
#include <functional>
#include <memory>

class QIODevice;
class QLocalSocket;
class QSslSocket;
class QTimer;

namespace tui
{
	struct HttpRequest
	{
		QByteArray method = "GET";
		QString path;                  // including the query string
		QByteArray body;
		QByteArray contentType;
	};

	struct HttpResponse
	{
		int status = 0;                // 0 = transport failure, see error
		QHash<QByteArray, QByteArray> headers; // lower-cased names
		QByteArray body;
		QString error;
		bool ok() const { return status >= 200 && status < 300; }
	};

	// Incremental HTTP/1.1 response parser (Content-Length, chunked, or until close)
	class HttpParser
	{
	public:
		enum State { Headers, Body, ChunkSize, ChunkData, ChunkTrailer, Done, Failed };

		void reset();
		// Returns true once the response is complete
		bool feed(const QByteArray& data);
		// For a response delimited by the end of the stream
		bool finishOnClose();
		bool failed() const { return m_state == Failed; }
		bool hasData() const { return m_started; }
		bool keepAlive() const { return m_keepAlive; }
		const HttpResponse& response() const { return m_response; }
		// Bytes after the end of the response (a misbehaving server); ignored by the client
		QByteArray leftover() const { return m_buf; }

	private:
		bool parseHeaders();
		State m_state = Headers;
		QByteArray m_buf;
		HttpResponse m_response;
		qint64 m_remaining = -1;
		bool m_started = false, m_keepAlive = true, m_untilClose = false;
	};

	struct Endpoint
	{
		enum Kind { Unix, Tcp, Tls } kind = Unix;
		QString socketPath;            // Unix
		QString host;
		quint16 port = 0;
		QByteArray authorization;      // complete header value, empty for none
		bool insecure = false;         // skip certificate verification
		QSet<QString> trustedFingerprints;
		QString hostHeader() const;

		// http://host:port, https://host:port or unix:/path
		static bool parse(const QString& url, Endpoint& out, QString* error = nullptr);
	};

	// A single persistent connection with a FIFO of requests
	class HttpClient : public QObject
	{
		Q_OBJECT
	public:
		using Callback = std::function<void(const HttpResponse&)>;

		explicit HttpClient(QObject* parent = nullptr);
		~HttpClient() override;

		void setEndpoint(const Endpoint& ep);
		const Endpoint& endpoint() const { return m_ep; }
		void setTimeoutMs(int ms) { m_timeoutMs = ms; }

		void request(const HttpRequest& req, Callback cb);
		void request(const QByteArray& method, const QString& path, Callback cb, const QByteArray& body = QByteArray());

		// A certificate the system does not trust: the UI answers with acceptCertificate() or rejectCertificate()
		void acceptCertificate();
		void rejectCertificate();
		bool waitingForCertificateDecision() const { return m_certPending; }
		bool isConnected() const;
		int pendingCount() const { return int(m_queue.size()) + (m_current ? 1 : 0); }
		void abortAll(const QString& reason);

	signals:
		void certificateUntrusted(const QString& subject, const QString& fingerprint, const QStringList& problems);
		void connectionChanged(bool connected);

	private:
		struct Pending { HttpRequest req; Callback cb; bool retried = false; };

		void pump();
		void connectSocket();
		void sendCurrent();
		void onData();
		void onSocketClosed(const QString& err);
		void finishCurrent(const HttpResponse& r);
		void dropSocket();
		QByteArray serialize(const HttpRequest& r) const;

		Endpoint m_ep;
		QIODevice* m_sock = nullptr;
		std::deque<Pending> m_queue;
		std::unique_ptr<Pending> m_current;
		HttpParser m_parser;
		QTimer* m_timer;
		int m_timeoutMs = 15000;
		bool m_connected = false, m_certPending = false, m_sentOnThisConn = false, m_gotBytes = false;
		QString m_certFingerprint;
		QList<QSslError> m_lastSslErrors;
	};
}
