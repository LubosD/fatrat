#include "MHDDaemon.h"
#include "RuntimeException.h"
#include "Logger.h"
#include <QFile>
#include <cassert>

static const char* PAGE_AUTH_REQUIRED = "<html><body><h1>401 Authorization Required</body></html>";
static const long long MAX_POST_REQUEST = 10*1024*1024;

MHDDaemon::MHDDaemon()
: m_daemon(0)
{
}

MHDDaemon::~MHDDaemon()
{
	stop();
	foreach (Handler h, m_handlers)
	{
		delete h.handler;
	}
	m_handlers.clear();
}

void MHDDaemon::start(int port)
{
	stop();

	m_daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, port, 0, 0, MHDDaemon::process, this,
			MHD_OPTION_THREAD_POOL_SIZE, 3,
			MHD_OPTION_NOTIFY_COMPLETED, MHDDaemon::finished, this,
			MHD_OPTION_EXTERNAL_LOGGER, MHDDaemon::log, this,
			MHD_OPTION_END);
	if (!m_daemon)
		throw RuntimeException(tr("Error starting httpd"));
}

void MHDDaemon::startSSL(int port, QString pemFile)
{
	stop();

	QByteArray pemData = QFile(pemFile).readAll();

	m_daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY | MHD_USE_SSL, port, 0, 0, MHDDaemon::process, this,
			MHD_OPTION_THREAD_POOL_SIZE, 3,
			MHD_OPTION_NOTIFY_COMPLETED, MHDDaemon::finished, this,
			MHD_OPTION_EXTERNAL_LOGGER, MHDDaemon::log, this,
			MHD_OPTION_HTTPS_MEM_KEY, pemData.constData(),
			MHD_OPTION_HTTPS_MEM_CERT, pemData.constData(),
			MHD_OPTION_END);

	if (!m_daemon)
		throw RuntimeException(tr("Error starting httpd"));
}

void MHDDaemon::log(void* t, const char* fmt, va_list ap)
{
	char* message = new char[512];
	vsnprintf(message, 512, fmt, ap);

	Logger::global()->enterLogMessage("HttpService", message);

	delete [] message;
}

void MHDDaemon::stop()
{
	if (m_daemon)
	{
		MHD_stop_daemon(m_daemon);
		m_daemon = 0;
	}
}

void MHDDaemon::setPassword(QString password)
{
	m_strPassword = password;
}

void MHDDaemon::addHandler(QRegExp regexp, MHDService* handler)
{
	Handler h = { regexp, handler };
	m_handlers << h;
}

int MHDDaemon::process(void* t, struct MHD_Connection* conn, const char* url, const char* method, const char* version, const char* upload_data, size_t* upload_data_size, void** ptr)
{
	State* state = static_cast<State*>(*ptr);
	MHDDaemon* This = static_cast<MHDDaemon*>(t);
	MHDConnection c(conn);

	if (!state)
	{
		*ptr = state = new State;

		if (!This->m_strPassword.isEmpty())
		{
			char* password;
			char* username;
			bool bad;

			username = MHD_basic_auth_get_username_password(conn, &password);
			bad = !username || This->m_strPassword != password;

			free(password);
			free(username);

			if (bad)
			{
				struct MHD_Response* response;

				response = MHD_create_response_from_buffer(strlen(PAGE_AUTH_REQUIRED), const_cast<char*>(PAGE_AUTH_REQUIRED), MHD_RESPMEM_PERSISTENT);
				MHD_queue_basic_auth_fail_response(conn, "FatRat Web Interface", response);
				MHD_destroy_response(response);

				return MHD_YES;
			}
		}

		// Find the right handler
		state->handler = 0;

		foreach (Handler h, This->m_handlers)
		{
			if (h.regexp.exactMatch(url))
				state->handler = h.handler;
		}

		if (!state->handler)
		{
			// No handler -> error 404
			return c.showErrorPage(MHD_HTTP_NOT_FOUND, "Not Found");
		}

		return MHD_YES;
	}
	else
	{

		// TODO: use the handler
		if (!strcasecmp(method, MHD_HTTP_METHOD_GET))
			state->handler->handleGet(url, c);
		else if (!strcasecmp(method, MHD_HTTP_METHOD_POST))
		{
			if (*upload_data_size)
			{
				// process incoming POST data
				if (state->postData.isEmpty())
				{
					// check max length
					long long len = c.headers()["Content-Length"].toLongLong();
					if (len > MAX_POST_REQUEST)
						return c.showErrorPage(MHD_HTTP_REQUEST_ENTITY_TOO_LARGE, "Request Entity Too Large");
				}
				state->postData.append(upload_data, *upload_data_size);
				*upload_data_size = 0;

				if (state->postData.size() > MAX_POST_REQUEST)
					return c.showErrorPage(MHD_HTTP_REQUEST_ENTITY_TOO_LARGE, "Request Entity Too Large");
			}
			else
			{
				// data read, invoke the handler
				state->handler->handlePost(url, c, state->postData);
			}
		}
		else if (!strcasecmp(method, MHD_HTTP_METHOD_HEAD))
			state->handler->handleHead(url, c);
		else
		{
			// return unsupported method
			return c.showErrorPage(MHD_HTTP_METHOD_NOT_ALLOWED, "Method Not Allowed");
		}

		return MHD_YES;
	}
}

void MHDDaemon::finished(void* t, struct MHD_Connection* conn, void** ptr, enum MHD_RequestTerminationCode toe)
{
	State* state = static_cast<State*>(*ptr);
	delete state;
}

