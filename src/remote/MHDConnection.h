#ifndef FR_MHDCONNECTION_H
#define FR_MHDCONNECTION_H

#include <microhttpd.h>
#include <QString>
#include <QMap>

class MHDConnection
{
protected:
	MHDConnection(MHD_Connection* conn);
public:
	int showErrorPage(int code, const char* message);

	// Use only with doneWriting()
	void write(const void* data, size_t len);
	void doneWriting(const char* mimeType = 0);

	MHD_Connection* getConn() { return m_conn; }

	QMap<QString,QString> getParameters() const { return fields(MHD_GET_ARGUMENT_KIND); }
	QMap<QString,QString> headers() const { return fields(MHD_HEADER_KIND); }
	QMap<QString,QString> cookies() const { return fields(MHD_COOKIE_KIND); }
protected:
	QMap<QString,QString> fields(enum MHD_ValueKind kind) const;
private:
	MHD_Connection* m_conn;
	void* m_buffer;
	size_t m_length, m_capacity;

	friend class MHDDaemon;
};

#endif

