#include "MHDFileService.h"
#include <QFileInfo>
#include <QFile>

static const size_t READ_BUFFER = 32*1024;

static ssize_t readCallback(void* ptr, uint64_t pos, char* buf, size_t max);
struct CBData
{
        QFile* file;
        qint64 rangeFrom, rangeTo;
};

MHDFileService::MHDFileService(QString baseDirectory)
	: m_strBaseDirectory(baseDirectory)
{
	if (!m_strBaseDirectory.endsWith('/'))
		m_strBaseDirectory += '/';

	m_magic = magic_open(MAGIC_SYMLINK | MAGIC_MIME_TYPE | MAGIC_ERROR);
}

MHDFileService::~MHDFileService()
{
	magic_close(m_magic);
}

void MHDFileService::handleGet(QString url, MHDConnection& conn)
{
	if (url.contains("/.."))
	{
		conn.showErrorPage(MHD_HTTP_FORBIDDEN, "Forbidden");
		return;
	}

	QFileInfo info(m_strBaseDirectory + url);
	if (!info.exists())
		conn.showErrorPage(MHD_HTTP_NOT_FOUND, "Not Found");
	else if (!info.isFile())
		conn.showErrorPage(MHD_HTTP_FORBIDDEN, "Forbidden");
	else
	{
		// Check for HTTP ranges
		QString range = conn.headers()[MHD_HTTP_HEADER_RANGE];
		qint64 rangeFrom = -1, rangeTo = -1;

		if (!range.isEmpty())
		{
			QRegExp re("bytes=(\\d*)-(\\d*)$");
			if (re.indexIn(range) != -1)
			{
				QString from, to;
				from = re.cap(1);
				to = re.cap(2);

				if (!from.isEmpty())
					rangeFrom = from.toLongLong();
				else
					rangeFrom = 0;

				if (!to.isEmpty())
					rangeTo = to.toLongLong();
				else
					rangeTo = info.size()-1;

				if (from < 0 || to < 0 || to >= info.size())
				{
					conn.showErrorPage(MHD_HTTP_REQUESTED_RANGE_NOT_SATISFIABLE, "Requested Range Not Satisfiable");
					return;
				}
			}
		}

		QFile file(info.filePath());
		if (!file.open(QIODevice::ReadOnly))
		{
			conn.showErrorPage(MHD_HTTP_FORBIDDEN, "Forbidden");
			return;
		}

		// Detect MIME type
		QByteArray path = info.filePath().toUtf8();
		const char* mimeType = magic_file(m_magic, path.constData());
		struct MHD_Response* response;
		int code;
		CBData cbdata;

		cbdata.file = &file;

		if (rangeFrom == -1 && rangeTo == -1)
		{
			cbdata.rangeFrom = 0;
			cbdata.rangeTo = info.size();
			response = MHD_create_response_from_callback(cbdata.rangeTo, READ_BUFFER, readCallback, &cbdata, 0);
			code = MHD_HTTP_OK;
		}
		else
		{
			if (rangeFrom > 0)
				file.seek(rangeFrom);
			cbdata.rangeFrom = rangeFrom;
			cbdata.rangeTo = rangeTo;
			response = MHD_create_response_from_callback(cbdata.rangeTo - cbdata.rangeFrom, READ_BUFFER, readCallback, &cbdata, 0);
			code = MHD_HTTP_PARTIAL_CONTENT;
		}

		if (mimeType)
			MHD_add_response_header(response, "Content-Type", mimeType);
		MHD_add_response_header(response, "Server: FatRat " VERSION);
		
		MHD_queue_response(m_conn, code, response);
		MHD_destroy_response(response);	
	}
}

ssize_t readCallback(void* ptr, uint64_t pos, char* buf, size_t max)
{
	CBData* data = static_cast<CBData*>(ptr);
	qint64 remaining = data->rangeTo - data->rangeFrom - pos;

	if (max > remaining)
		max = remaining;

	if (!max)
		return -1;

	qint64 read = data->file->read(buf, max);
	if (read < 0)
		return -2;
	else
		return read;
}

