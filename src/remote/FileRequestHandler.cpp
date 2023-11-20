#include "FileRequestHandler.h"

#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>

#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <cstring>

FileRequestHandler::FileRequestHandler(const char* prefix, const char* root)
    : m_prefix(prefix), m_root(root) {}

void FileRequestHandler::run() {
  static QMimeDatabase mimeDB;
  std::string uri = request().getURI();
  std::string fpath;
  size_t qpos;

  if (uri.find("..") != std::string::npos) {
    sendErrorResponse(Poco::Net::HTTPResponse::HTTPStatus::HTTP_NOT_FOUND,
                      "Not Found");
    return;
  }

  qpos = uri.find('?');
  if (qpos != std::string::npos) uri.resize(qpos);

  fpath = m_root;
  fpath += uri.substr(strlen(m_prefix));

  QFileInfo fi(QString::fromStdString(fpath));

  if (!fi.exists()) {
    sendErrorResponse(Poco::Net::HTTPResponse::HTTPStatus::HTTP_NOT_FOUND,
                      "Not Found");
    return;
  }

  if (fi.isDir()) {
    fpath += "/index.html";
    fi = QFileInfo(QString::fromStdString(fpath));

    if (!fi.exists() || fi.isDir()) {
      sendErrorResponse(Poco::Net::HTTPResponse::HTTPStatus::HTTP_FORBIDDEN,
                        "Forbidden");
      return;
    }
  }

  QString mimeType;

  if (fi.absoluteFilePath().endsWith(".html"))
    mimeType = QLatin1String("text/html");
  else
    mimeType = mimeDB.mimeTypeForFile(fi.absoluteFilePath()).name();

  response().sendFile(fi.absoluteFilePath().toStdString(),
                      mimeType.toStdString());
}
