#include "HttpClient.h"
#include <QCryptographicHash>
#include <QLocalSocket>
#include <QSslCertificate>
#include <QSslError>
#include <QSslSocket>
#include <QTcpSocket>
#include <QTimer>

namespace tui
{

// ---------------------------------------------------------------- parser

void HttpParser::reset()
{
	m_state = Headers;
	m_buf.clear();
	m_response = HttpResponse();
	m_remaining = -1;
	m_started = false;
	m_keepAlive = true;
	m_untilClose = false;
}

bool HttpParser::parseHeaders()
{
	int end = m_buf.indexOf("\r\n\r\n");
	if (end < 0)
	{
		if (m_buf.size() > 64 * 1024)
			m_state = Failed;
		return false;
	}

	const QList<QByteArray> lines = m_buf.left(end).split('\n');
	m_buf.remove(0, end + 4);

	const QList<QByteArray> status = lines.value(0).trimmed().split(' ');
	if (status.size() < 2 || !status[0].startsWith("HTTP/"))
	{
		m_state = Failed;
		m_response.error = "Malformed HTTP response";
		return false;
	}
	m_response.status = status[1].toInt();
	const bool http10 = status[0] == "HTTP/1.0";
	m_keepAlive = !http10;

	for (int i = 1; i < lines.size(); i++)
	{
		int colon = lines[i].indexOf(':');
		if (colon <= 0)
			continue;
		m_response.headers[lines[i].left(colon).trimmed().toLower()] = lines[i].mid(colon + 1).trimmed();
	}

	const QByteArray conn = m_response.headers.value("connection").toLower();
	if (conn == "close")
		m_keepAlive = false;
	else if (conn == "keep-alive")
		m_keepAlive = true;

	const bool noBody = (m_response.status >= 100 && m_response.status < 200) || m_response.status == 204 || m_response.status == 304;
	if (noBody)
	{
		m_state = Done;
	}
	else if (m_response.headers.value("transfer-encoding").toLower().contains("chunked"))
	{
		m_state = ChunkSize;
	}
	else if (m_response.headers.contains("content-length"))
	{
		bool ok;
		m_remaining = m_response.headers["content-length"].toLongLong(&ok);
		if (!ok || m_remaining < 0)
		{
			m_state = Failed;
			m_response.error = "Bad Content-Length";
			return false;
		}
		m_state = m_remaining == 0 ? Done : Body;
	}
	else
	{
		m_untilClose = true;
		m_keepAlive = false;
		m_state = Body;
	}
	return true;
}

bool HttpParser::feed(const QByteArray& data)
{
	if (!data.isEmpty())
		m_started = true;
	m_buf += data;

	for (;;)
	{
		switch (m_state)
		{
			case Headers:
				if (!parseHeaders())
					return m_state == Done;
				break;
			case Body:
				if (m_untilClose)
				{
					m_response.body += m_buf;
					m_buf.clear();
					return false;
				}
				else
				{
					qint64 take = qMin<qint64>(m_remaining, m_buf.size());
					m_response.body += m_buf.left(take);
					m_buf.remove(0, take);
					m_remaining -= take;
					if (m_remaining == 0)
						m_state = Done;
					else
						return false;
				}
				break;
			case ChunkSize:
			{
				int eol = m_buf.indexOf("\r\n");
				if (eol < 0)
					return false;
				bool ok;
				m_remaining = m_buf.left(eol).split(';')[0].trimmed().toLongLong(&ok, 16);
				m_buf.remove(0, eol + 2);
				if (!ok || m_remaining < 0)
				{
					m_state = Failed;
					m_response.error = "Bad chunk size";
					return false;
				}
				m_state = m_remaining == 0 ? ChunkTrailer : ChunkData;
				break;
			}
			case ChunkData:
			{
				// the data is followed by CRLF
				if (m_buf.size() < m_remaining + 2)
				{
					qint64 take = qMin<qint64>(m_remaining, m_buf.size());
					m_response.body += m_buf.left(take);
					m_buf.remove(0, take);
					m_remaining -= take;
					return false;
				}
				m_response.body += m_buf.left(m_remaining);
				m_buf.remove(0, m_remaining + 2);
				m_state = ChunkSize;
				break;
			}
			case ChunkTrailer:
			{
				int eol = m_buf.indexOf("\r\n");
				if (eol < 0)
					return false;
				bool last = eol == 0;
				m_buf.remove(0, eol + 2);
				if (last)
					m_state = Done;
				break;
			}
			case Done:
				return true;
			case Failed:
				return false;
		}
	}
}

bool HttpParser::finishOnClose()
{
	if (m_state == Body && m_untilClose)
	{
		m_state = Done;
		return true;
	}
	return m_state == Done;
}

// -------------------------------------------------------------- endpoint

QString Endpoint::hostHeader() const
{
	if (kind == Unix)
		return "localhost";
	const bool def = (kind == Tcp && port == 80) || (kind == Tls && port == 443);
	const QString h = host.contains(':') ? "[" + host + "]" : host;
	return def ? h : QString("%1:%2").arg(h).arg(port);
}

bool Endpoint::parse(const QString& url, Endpoint& out, QString* error)
{
	auto fail = [&](const QString& m) { if (error) *error = m; return false; };

	if (url.startsWith("unix:"))
	{
		out.kind = Unix;
		out.socketPath = url.mid(5);
		if (out.socketPath.startsWith("//"))
			out.socketPath = out.socketPath.mid(2);
		return out.socketPath.isEmpty() ? fail("Empty socket path") : true;
	}

	QUrl u(url);
	if (!u.isValid() || u.host().isEmpty())
		return fail("Not a valid URL: " + url);
	if (u.scheme() == "http")
		out.kind = Tcp;
	else if (u.scheme() == "https")
		out.kind = Tls;
	else
		return fail("The URL must start with http://, https:// or unix:");

	out.host = u.host();
	out.port = quint16(u.port(out.kind == Tls ? 443 : 80));
	return true;
}

// ---------------------------------------------------------------- client

HttpClient::HttpClient(QObject* parent) : QObject(parent), m_timer(new QTimer(this))
{
	m_timer->setSingleShot(true);
	connect(m_timer, &QTimer::timeout, this, [this]() {
		const bool wasCert = m_certPending;
		if (wasCert)
			return; // the user is deciding
		dropSocket();
		if (m_current)
		{
			HttpResponse r;
			r.error = tr("Request timed out");
			finishCurrent(r);
		}
	});
}

HttpClient::~HttpClient()
{
	delete m_sock;
}

void HttpClient::setEndpoint(const Endpoint& ep)
{
	m_ep = ep;
	dropSocket();
}

bool HttpClient::isConnected() const
{
	return m_connected;
}

void HttpClient::dropSocket()
{
	m_timer->stop();
	if (m_sock)
	{
		QObject* s = m_sock;
		m_sock = nullptr;
		s->disconnect(this);
		if (auto* l = qobject_cast<QLocalSocket*>(s)) l->abort();
		else if (auto* a = qobject_cast<QAbstractSocket*>(s)) a->abort();
		s->deleteLater();
	}
	if (m_connected)
	{
		m_connected = false;
		emit connectionChanged(false);
	}
	m_certPending = false;
	m_sentOnThisConn = false;
	m_gotBytes = false;
}

void HttpClient::request(const QByteArray& method, const QString& path, Callback cb, const QByteArray& body)
{
	HttpRequest r;
	r.method = method;
	r.path = path;
	r.body = body;
	if (!body.isEmpty())
		r.contentType = "application/json";
	request(r, std::move(cb));
}

void HttpClient::request(const HttpRequest& req, Callback cb)
{
	m_queue.push_back({ req, std::move(cb), false });
	pump();
}

void HttpClient::abortAll(const QString& reason)
{
	dropSocket();
	HttpResponse r;
	r.error = reason;
	if (m_current)
		finishCurrent(r);
	while (!m_queue.empty())
	{
		Pending p = std::move(m_queue.front());
		m_queue.pop_front();
		if (p.cb)
			p.cb(r);
	}
}

void HttpClient::pump()
{
	if (m_current || m_queue.empty())
		return;

	m_current.reset(new Pending(std::move(m_queue.front())));
	m_queue.pop_front();
	m_parser.reset();

	if (!m_sock)
		connectSocket();
	else if (m_connected && !m_certPending)
		sendCurrent();
	// otherwise the pending connection will send it
}

QByteArray HttpClient::serialize(const HttpRequest& r) const
{
	QByteArray out = r.method + ' ' + r.path.toUtf8() + " HTTP/1.1\r\n";
	out += "Host: " + m_ep.hostHeader().toUtf8() + "\r\n";
	out += "User-Agent: fatrat-tui\r\nAccept: */*\r\nConnection: keep-alive\r\n";
	if (!m_ep.authorization.isEmpty())
		out += "Authorization: " + m_ep.authorization + "\r\n";
	if (!r.body.isEmpty() || r.method == "POST" || r.method == "PUT" || r.method == "PATCH")
	{
		if (!r.contentType.isEmpty())
			out += "Content-Type: " + r.contentType + "\r\n";
		out += "Content-Length: " + QByteArray::number(r.body.size()) + "\r\n";
	}
	out += "\r\n";
	out += r.body;
	return out;
}

void HttpClient::connectSocket()
{
	m_gotBytes = false;
	m_sentOnThisConn = false;
	m_timer->start(m_timeoutMs);

	auto onConnected = [this]() {
		m_connected = true;
		emit connectionChanged(true);
		if (m_current && !m_certPending)
			sendCurrent();
	};

	if (m_ep.kind == Endpoint::Unix)
	{
		auto* s = new QLocalSocket(this);
		m_sock = s;
		connect(s, &QLocalSocket::connected, this, onConnected);
		connect(s, &QLocalSocket::readyRead, this, &HttpClient::onData);
		connect(s, &QLocalSocket::disconnected, this, [this]() { onSocketClosed(QString()); });
		connect(s, &QLocalSocket::errorOccurred, this, [this, s](QLocalSocket::LocalSocketError) {
			if (s->state() == QLocalSocket::UnconnectedState)
				onSocketClosed(s->errorString());
		});
		s->connectToServer(m_ep.socketPath);
	}
	else if (m_ep.kind == Endpoint::Tcp)
	{
		auto* s = new QTcpSocket(this);
		m_sock = s;
		connect(s, &QTcpSocket::connected, this, onConnected);
		connect(s, &QTcpSocket::readyRead, this, &HttpClient::onData);
		connect(s, &QTcpSocket::disconnected, this, [this]() { onSocketClosed(QString()); });
		connect(s, &QTcpSocket::errorOccurred, this, [this, s](QAbstractSocket::SocketError) {
			onSocketClosed(s->errorString());
		});
		s->connectToHost(m_ep.host, m_ep.port);
	}
	else
	{
		auto* s = new QSslSocket(this);
		m_sock = s;
		connect(s, &QSslSocket::encrypted, this, [this, onConnected]() {
			if (m_certFingerprint.isEmpty() || m_ep.insecure || m_ep.trustedFingerprints.contains(m_certFingerprint))
				onConnected();
			else
			{
				// Handshake finished despite errors we chose to ignore: hold requests until the user decides
				m_certPending = true;
				m_timer->stop();
				auto* ss = static_cast<QSslSocket*>(m_sock);
				const QSslCertificate cert = ss->peerCertificate();
				QStringList problems;
				for (const QSslError& e : m_lastSslErrors)
					problems << e.errorString();
				emit certificateUntrusted(cert.subjectDisplayName() + QString(" (issuer: %1)").arg(cert.issuerDisplayName()), m_certFingerprint, problems);
			}
		});
		connect(s, &QSslSocket::sslErrors, this, [this, s](const QList<QSslError>& errors) {
			m_lastSslErrors = errors;
			const QByteArray digest = s->peerCertificate().digest(QCryptographicHash::Sha256).toHex(':').toUpper();
			m_certFingerprint = QString::fromLatin1(digest);
			s->ignoreSslErrors();
		});
		connect(s, &QSslSocket::readyRead, this, &HttpClient::onData);
		connect(s, &QSslSocket::disconnected, this, [this]() { onSocketClosed(QString()); });
		connect(s, &QSslSocket::errorOccurred, this, [this, s](QAbstractSocket::SocketError) {
			onSocketClosed(s->errorString());
		});
		m_certFingerprint.clear();
		m_lastSslErrors.clear();
		s->connectToHostEncrypted(m_ep.host, m_ep.port);
	}
}

void HttpClient::acceptCertificate()
{
	if (!m_certPending)
		return;
	m_certPending = false;
	m_ep.trustedFingerprints.insert(m_certFingerprint);
	m_connected = true;
	emit connectionChanged(true);
	m_timer->start(m_timeoutMs);
	if (m_current)
		sendCurrent();
}

void HttpClient::rejectCertificate()
{
	if (!m_certPending)
		return;
	abortAll(tr("The server certificate was not accepted"));
}

void HttpClient::sendCurrent()
{
	if (!m_sock || !m_current)
		return;
	m_sentOnThisConn = true;
	m_gotBytes = false;
	m_timer->start(m_timeoutMs);
	m_sock->write(serialize(m_current->req));
}

void HttpClient::onData()
{
	if (!m_sock)
		return;
	const QByteArray data = m_sock->readAll();
	if (!m_current)
		return; // stray bytes on an idle connection
	if (!data.isEmpty())
		m_gotBytes = true;

	const bool done = m_parser.feed(data);
	if (m_parser.failed())
	{
		HttpResponse r;
		r.error = m_parser.response().error.isEmpty() ? tr("Malformed HTTP response") : m_parser.response().error;
		dropSocket();
		finishCurrent(r);
		return;
	}
	if (done)
	{
		const bool keep = m_parser.keepAlive();
		HttpResponse r = m_parser.response();
		if (!keep)
			dropSocket();
		else
			m_timer->stop();
		finishCurrent(r);
	}
}

void HttpClient::onSocketClosed(const QString& err)
{
	if (!m_sock)
		return;

	if (m_current && m_parser.hasData() && m_parser.finishOnClose())
	{
		HttpResponse r = m_parser.response();
		dropSocket();
		finishCurrent(r);
		return;
	}

	const bool hadConnection = m_connected;
	const bool retry = m_current && !m_current->retried && hadConnection && m_sentOnThisConn && !m_gotBytes;
	dropSocket();

	if (!m_current)
		return;

	if (retry)
	{
		// the server closed an idle keep-alive connection: resend on a fresh one
		m_current->retried = true;
		connectSocket();
		return;
	}

	HttpResponse r;
	r.error = err.isEmpty() ? tr("Connection closed by the server") : err;
	finishCurrent(r);
}

void HttpClient::finishCurrent(const HttpResponse& r)
{
	m_timer->stop();
	std::unique_ptr<Pending> p = std::move(m_current);
	if (p && p->cb)
		p->cb(r);
	pump();
}

}
