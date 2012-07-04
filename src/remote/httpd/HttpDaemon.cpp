#include "HttpDaemon.h"
#include "HttpResponse.h"
#include "HttpHandler.h"
#include "RuntimeException.h"
#include <QRegExp>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <boost/scoped_array.hpp>
#include <netdb.h>
#include <algorithm>
#include <cassert>

const int READ_BUFFER_LENGTH = 4*1024;
const int MAX_URLENCODED_BODY = 1024;
const int EVENTS_PER_CYCLE = 5;
const int SOCKET_BACKLOG = 5;

HttpDaemon::HttpDaemon()
: m_server(0), m_poller(0), m_bUseV6(false), m_port(2233)
{
}

HttpDaemon::~HttpDaemon()
{
    stop();
}

void HttpDaemon::registerHandler(QString regexp, HttpHandler *handler)
{
    Handler h = { regexp, handler };
    m_handlers << h;
}

void HttpDaemon::startV4()
{
	m_server = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_server < 0)
	        throwErrnoException();

	sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_port = htons(m_port);

	if (m_strBindAddress.isEmpty())
        sa.sin_addr.s_addr = INADDR_ANY;
	else
	{
		addrinfo hints;
        addrinfo* res;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        QByteArray ba = m_strBindAddress.toUtf8();
        if (int e = getaddrinfo(ba.constData(), 0, &hints, &res))
            throw RuntimeException(gai_strerror(e));
        sa.sin_addr = reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr;
	}

	if (::bind(m_server, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) < 0)
		throwErrnoException();
}

void HttpDaemon::startV6()
{
	m_server = ::socket(AF_INET6, SOCK_STREAM, 0);
	if (m_server < 0)
			throwErrnoException();

	sockaddr_in6 sa;
	sa.sin6_family = AF_INET6;
	sa.sin6_port = htons(m_port);

	if (m_strBindAddress.isEmpty())
		sa.sin6_addr = in6addr_any;
	else
	{
		addrinfo hints;
		addrinfo* res;

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET6;
		hints.ai_socktype = SOCK_STREAM;

        QByteArray ba = m_strBindAddress.toUtf8();
		if (int e = getaddrinfo(ba.constData(), 0, &hints, &res))
			throw RuntimeException(gai_strerror(e));
        sa.sin6_addr = reinterpret_cast<sockaddr_in6*>(res->ai_addr)->sin6_addr;
	}

	if (::bind(m_server, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) < 0)
		throwErrnoException();
}

void* HttpDaemon::pollThread(void* t)
{
	HttpDaemon* This = static_cast<HttpDaemon*>(t);

	// Add the listening socket to the fd set
	This->m_poller->addSocket(This->m_server, Poller::PollerIn | Poller::PollerError);
	
    while (This->m_server)
	{
        if (!This->pollCycle())
			break;
	}

	return 0;
}

bool HttpDaemon::pollCycle()
{
	Poller::Event ev[EVENTS_PER_CYCLE];
	int nfds;

    nfds = m_poller->wait(500, ev, EVENTS_PER_CYCLE);

	for (int i = 0; i < nfds; i++)
	{
        const Poller::Event& e = ev[i];
        if (e.socket == m_server)
		{
			if (e.flags & Poller::PollerError)
				return false;
			acceptClient();
 		}
        else if (m_clients.contains(e.socket))
		{
			if (e.flags & Poller::PollerError)
				closeClient(e.socket);
			else if (e.flags & Poller::PollerIn)
				readClient(e.socket);
			else if (e.flags & Poller::PollerOut)
				writeClient(e.socket);
		}
		else
		{
			assert(m_clients.contains(e.socket));
			// This should never happen
			m_poller->removeSocket(e.socket);
			::close(e.socket);
		}
	}
	return true;
}

void HttpDaemon::closeClient(int s)
{
	m_poller->removeSocket(s);
	::close(s);
    m_clients.remove(s);
}

void HttpDaemon::parseHTTPRequest(HttpRequest& request, const QByteArray& req)
{
	QList<QByteArray> lines;

	lines = req.split('\n');

	// Parse request line
	QList<QByteArray> seg = lines[0].split(' ');

	if (lines.size() < 2 || seg.size() != 3)
	{
		// send bad request
		response->sendErrorGeneric(400, "Bad Request");
		closeClient(s);
		return true;
	}

	request.method = seg[0];
	request.uri = seg[1];

	{
		int pos = request.uri.indexOf('?');
		QByteArray vars = request.uri.mid(pos+1).toLatin1();

		request.getVars = parseUE(vars);
		request.uri.truncate(pos); // remove query string
	}

    // Parse request headers
    for (int i=1; i<lines.size()-1; i++)
    {
        QByteArray line = lines[i].trimmed();
        int pos = line.indexOf(": ");

        if (pos < 0)
            continue;

        request.headers[line.left(pos).toLower()] = line.mid(pos+2);
    }

}

bool HttpDaemon::tryProcessRequest(int s)
{
	assert(m_clients.contains(s));

	Client& c = m_clients[s];
    int pos;
    QByteArray req;
    HttpRequest& request = c.request;
    boost::shared_ptr<HttpResponse> response;

	assert(c.state == Client::ReceivingHeaders);

    // detect end-of-request (double \r\n)
    pos = c.requestBuffer.indexOf("\r\n\r\n");

    if (pos < 0) // request not complete yet
		return false;

	// carve out the request
    req = c.requestBuffer.left(pos+4);

	// this may be the body or another request
    c.remnantBuffer = c.requestBuffer.mid(pos+4);
	c.requestBuffer.clear();

	parseHTTPRequest(request, req);

    response = boost::shared_ptr<HttpResponse>( new HttpResponse(this, s) );

    // Check for request body presence
    c.requestBodyLength = request.headers["content-length"].toLongLong();

    // POST and content length must go together
    if ((c.requestBodyLength > 0) != (request.method == "POST"))
    {
        // send bad request
        response->sendErrorGeneric(411, "Length Required");
        closeClient(s);
        return true;
    }

    if (!c.requestBodyLength) // No request body -> start handling
	{
		c.state = Client::Responding; 
	}
	else
	{
        if (request.headers["content-type"] == "application/x-www-form-urlencoded")
		{
			// handle locally
			c.state = Client::ReceivingUEBody;

			// check body length (max 1024 bytes)
			if (c.requestBodyLength > MAX_URLENCODED_BODY)
			{
				response->sendErrorGeneric(413, "Request Entity Too Large");
				closeClient(s);
				return true;
			}
		}
		else
		{
			// handled in the handler
			c.state = Client::ReceivingBody;
		}
	}

	c.handler = findHandler(request.uri);

    if (!c.handler) // No handler found
    {
        if (request.method == "POST" && request.headers["expect"].compare("100-continue", Qt::CaseInsensitive) != 0)
        {
			// receive the body first, then fail
            return true;
        }

        // send 404
        response->sendErrorGeneric(404, "Not Found");
    }
    else if (c.state == Client::Responding) // Invoke the handler
    {
		c.handler->handle(request, response, &c.userPointer);
		// check if handler reacted

		if (!response->wasHandled())
			response->sendErrorGeneric(500, "Internal Server Error");
    }

	return true;
}

HttpHandler* HttpDaemon::findHandler(QString uri)
{
	int maxLen = 0; // prefer longer regexps
    HttpHandler* handler = 0;

    foreach (const Handler& h, m_handlers)
    {
        QRegExp re(h.regexp);
        if (re.exactMatch(request.uri) && h.regexp.size() > maxLen)
        {
            maxLen = h.regexp.size();
            handler = h.handler;
        }
    }

	return handler;
}

QMap<QString,QString> HttpDaemon::parseUE(QByteArray ba)
{
	QList<QByteArray> segs = ba.split('&');
	QMap<QString, QString> rv;

	for (int i = 0; i < segs.size(); i++)
	{
		QString name, value;
		int pos = segs[i].indexOf('=');

		if (pos < 0)
			continue;

		name = QByteArray::fromPercentEncoding(segs[i].left(pos));
		value = QByteArray::fromPercentEncoding(segs[i].mid(pos+1));
		rv[name] = value;
	}

	return rv;
}

void HttpDaemon::readClient(int s)
{
	assert(m_clients.contains(s));

	Client& c = m_clients[s];
    boost::scoped_array<char> buf(new char[READ_BUFFER_LENGTH]);
    int rd;

	assert(c.state != Client::Responding); // TODO: how is this enforced?

	while (c.state != Client::Responding && (rd = readBytes(s, buf.get(), READ_BUFFER_LENGTH)) > 0)
	{
		switch (c.state)
		{
		case Client::ReceivingBody:
		case Client::ReceivingUEBody:
		{
			long long toProcess = std::min<long long>(rd, c.requestBodyLength - c.requestBodyReceived);

			// Drop data if no handler (delayed 404)
			if (c.handler)
			{
				if (c.state == Client::ReceivingBody)
					c.handler->write(&c.userPointer, c.request, buf.get(), size_t(toProcess));
				else
					c.requestBodyReceived += toProcess;
			}

			c.requestBodyReceived += toProcess;

			if (rd > toProcess)
				c.remnantBuffer.append(buf.get() + toProcess, rd-toProcess);

			assert(c.requestBodyReceived <= c.requestBodyLength);

	        if (c.requestBodyReceived == c.requestBodyLength)
    	    {
                boost::shared_ptr<HttpResponse> response = boost::shared_ptr<HttpResponse>( new HttpResponse(this, s) );

				c.state = Client::Responding;

        	    // call handler
            	if (c.handler)
				{
					if (c.state == Client::ReceivingUEBody)
					{
						c.request.postVars = parseUE(c.requestBuffer);
						c.requestBuffer.clear();
					}
					
                	c.handler->handle(c.request, response, &c.userPointer);

					// check if handled
					if (!response->wasHandled())
						response->sendErrorGeneric(500, "Internal Server Error");
				}
	            else
    	        {
        	        // delayed POST 404
                    response->sendErrorGeneric(404, "Not Found");
	            }
    	    }
			break;
		}
		case Client::ReceivingHeaders:
		{
			c.requestBuffer.append(buf.get(), rd);
            tryProcessRequest(s);
			break;
		}
		default:
			assert(false);
		}
	}

	if (rd < 0)
	{
		// TODO: log error
		closeClient(s);
	}
}

void HttpDaemon::writeClient(int s)
{
	assert(m_clients.contains(s));

	Client& c = m_clients[s];

	assert(c.state == Client::Responding);

	// TODO: Get current response mode
	// If callback, call callback
	// If simple, write buffer contents
	// If async, we shouldn't be here
}

int HttpDaemon::readBytesOrRemnant(int s, char* buffer, int max)
{
	assert(m_clients.contains(s));
	Client& c = m_clients[s];

	if (!c.remnantBuffer.isEmpty())
	{
		int out = std::min<int>(max, c.remnantBuffer.size());
		memcpy(buffer, c.remnantBuffer.constData(), out);

		if (out < c.remnantBuffer.size())
			c.remnantBuffer = c.remnantBuffer.mid(out);
		else
			c.remnantBuffer.clear();

		return out;
	}
	else
	{
		return readBytes(s, buffer, max);
	}
}

void HttpDaemon::acceptClient()
{
	boost::shared_ptr<sockaddr> sa;
	socklen_t len;

    if (m_bUseV6)
	{
		len = sizeof(sockaddr_in6);
        sa = boost::shared_ptr<sockaddr>( reinterpret_cast<sockaddr*>(new sockaddr_in6) );
	}
	else
	{
		len = sizeof(sockaddr_in);
        sa = boost::shared_ptr<sockaddr>( reinterpret_cast<sockaddr*>(new sockaddr_in) );
	}

	int s = ::accept(m_server, sa.get(), &len);
	if (s > 0)
	{
		Client c;
		c.addr = sa;
		c.state = Client::ReceivingHeaders;
		m_clients[s] = c;

		setFdUnblocking(s, true);
		m_poller->addSocket(s, Poller::PollerIn | Poller::PollerError);
	}
}

void HttpDaemon::setFdUnblocking(int fd, bool unblocking)
{
	::fcntl(fd, F_SETFL, unblocking ? O_NONBLOCK : 0);
}

void HttpDaemon::start()
{
	if (m_bUseV6)
		startV6();
	else
		startV4();

	if (::listen(m_server, SOCKET_BACKLOG) < 0)
		throwErrnoException();

	m_poller = Poller::createInstance(this);
	pthread_create(&m_thread, 0, HttpDaemon::pollThread, this); 
}

void HttpDaemon::stop()
{
	if (m_server > 0)
	{
		::close(m_server);
		m_server = 0;

        pthread_join(m_thread, 0);

		delete m_poller;
		m_poller = 0;

		foreach (int s, m_client.keys())
			::close(s);

		m_clients.clear();
	}
}

