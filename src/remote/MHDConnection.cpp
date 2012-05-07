#include "config.h"
#include "MHDConnection.h"
#include <malloc.h>

MHDConnection::MHDConnection(MHD_Connection* conn)
	: m_conn(conn), m_buffer(0), m_length(0), m_capacity(0)
{
}

int MHDConnection::showErrorPage(int code, const char* message)
{
	struct MHD_Response* response;
	char* buf = malloc(strlen(message) + 50);

	sprintf(buf, "<html><body><h1>%d %s</body></html>", code, message);

	response = MHD_create_response_from_buffer(strlen(buf), buf, MHD_RESPMEM_MUST_FREE);
	MHD_add_response_header(response, "Server: FatRat " VERSION);

	MHD_queue_response(conn, code, response);
	MHD_destroy_response(respone);
	return MHD_YES;
}

void MHDConnection::write(const void* data, size_t len)
{
	if (len+m_length > m_capacity)
	{
		if (m_capacity)
			m_capacity *= 2;
		else
		{
			m_capacity = m_length;
			if (m_capacity < 4096)
				m_capacity = 4096;
		}
		m_buffer = realloc(m_buffer, m_capacity);
	}

	memcpy(m_buffer + m_length, data, len);
	m_length += len;
}

void MHDConnection::doneWriting(const char* mimeType)
{
	struct MHD_Response* response;
	response = MHD_create_response_from_buffer(m_length, m_buffer, MHD_RESPMEM_MUST_FREE);

	if (mimeType)
		MHD_add_response_header(response, "Content-Type", mimeType);
	MHD_add_response_header(response, "Server: FatRat " VERSION);

	MHD_queue_response(m_conn, MHD_HTTP_OK, response);
	MHD_destroy_response(response);
}

static int key_callback(QMap<QString,QString>* map, enum MHD_ValueKind kind, const char* key, const char* value)
{
	(*map)[key] = value;
	return MHD_YES;
}

QMap<QString,QString> MHDConnection::fields(enum MHD_ValueKind kind)
{
	QMap<QString,QString> rv;
	MHD_get_connection_values(m_conn, kind, key_callback, &rv);
	return rv;
}

