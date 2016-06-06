#include "FileRequestHandler.h"
#include <QFileInfo>
#include <QFile>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <QMimeDatabase>

FileRequestHandler::FileRequestHandler(const char* root)
	: m_root(root)
{

}

void FileRequestHandler::run()
{
	static QMimeDatabase mimeDB;
	std::string uri = request().getURI();
	std::string fpath;

	if (uri.find("..") != std::string::npos)
	{
		sendErrorResponse(Poco::Net::HTTPResponse::HTTPStatus::HTTP_NOT_FOUND, "Not Found");
		return;
	}

	fpath = m_root;
	fpath += uri;

	QFileInfo fi(QString::fromStdString(fpath));

	if (!fi.exists())
	{
		sendErrorResponse(Poco::Net::HTTPResponse::HTTPStatus::HTTP_NOT_FOUND, "Not Found");
		return;
	}

	if (fi.isDir())
	{
		fpath += "/index.html";
		fi = QFileInfo(QString::fromStdString(fpath));

		if (!fi.exists() || fi.isDir())
		{
			sendErrorResponse(Poco::Net::HTTPResponse::HTTPStatus::HTTP_FORBIDDEN, "Forbidden");
			return;
		}
	}

	QString mimeType = mimeDB.mimeTypeForFile(fi.absoluteFilePath()).name();
	response().sendFile(fi.absoluteFilePath().toStdString(), mimeType.toStdString());
}
