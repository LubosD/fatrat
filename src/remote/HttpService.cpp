/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2010 Lubos Dolezel <lubos a dolezel.info>

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

#include "HttpService.h"

#include <Poco/Net/Context.h>
#include <Poco/Net/HTMLForm.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/SecureServerSocket.h>
#include <Poco/Net/WebSocket.h>
#include <locale.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMultiMap>
#include <QPainter>
#include <QProcess>
#include <QSettings>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QtDebug>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <sstream>

#include "FileRequestHandler.h"
#include "Logger.h"
#include "Queue.h"
#include "RuntimeException.h"
#include "Settings.h"
#include "SettingsWebForm.h"
#include "SpeedGraph.h"
#include "Transfer.h"
#include "TransferFactory.h"
#include "config.h"
#include "dbus/DbusImpl.h"
#include "fatrat.h"
#include "remote/XmlRpcService.h"

extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;
extern QSettings* g_settings;
extern QVector<EngineEntry> g_enginesDownload;

Poco::SharedPtr<HttpService> HttpService::m_instance;

HttpService::HttpService() : m_port(0), m_server(nullptr) {
  m_instance = this;
  applySettings();

  SettingsItem si;
  si.lpfnCreate = SettingsWebForm::create;
  si.title = tr("Web Interface");
  si.icon = DelayedIcon(":/fatrat/webinterface.png");
  si.pfnApply = SettingsWebForm::applySettings;
  si.webSettingsScript = "/scripts/settings/webinterface.js";
  si.webSettingsIconURL = "/img/settings/webinterface.png";

  addSettingsPage(si);

  addHandler(QRegExp("/xmlrpc"),
             std::bind(&XmlRpcService::createHandler, &m_xmlRpc));
  addHandler(QRegExp("/log.*"),
             []() { return new LogService(QLatin1String("/log")); });
  addHandler(QRegExp("/subclass.*"),
             []() { return new SubclassService(QLatin1String("/subclass")); });
  addHandler(QRegExp("/browse.*"), []() {
    return new TransferTreeBrowserService(QLatin1String("/browse"));
  });
  addHandler(QRegExp("/download"), []() {
    return new TransferDownloadService(QLatin1String("/download"));
  });
  addHandler(QRegExp("/copyrights"), []() {
    return new FileRequestHandler("/copyrights", DATA_LOCATION "/README");
  });
  addHandler(QRegExp("/captcha"), []() { return new CaptchaService; });
  addHandler(QRegExp("/websocket"), []() { return new WebSocketService; });

  XmlRpcService::registerFunction(
      "HttpService.generateCertificate", generateCertificate,
      QVector<QVariant::Type>() << QVariant::String);

  connect(&m_timer, SIGNAL(timeout()), this, SLOT(keepalive()));
  m_timer.start(15 * 1000);
}

HttpService::~HttpService() {
  killCaptchaClients();

  m_instance = 0;
  if (m_server) {
    try {
      m_server->stopAll(true);
    } catch (...) {
    }
  }
}

void HttpService::killCaptchaClients() {
  QMutexLocker l(&m_registeredCaptchaClientsMutex);

  for (WebSocketService* s : m_registeredCaptchaClients) {
    s->terminate();
  }
  m_registeredCaptchaClients.clear();
}

void HttpService::applySettings() {
  try {
    qDebug() << "HttpService::applySettings()";
    bool enable = getSettingsValue("remote/enable").toBool();
    if (enable) {
      if (!m_server) {
        setup();
      } else {
        quint16 port = getSettingsValue("remote/port").toUInt();
        QString pemFile = getSettingsValue("remote/ssl_pem").toString();
        bool useSSL = getSettingsValue("remote/ssl").toBool();

        if (port != m_port || m_strSSLPem != pemFile || m_bUseSSL != useSSL ||
            (useSSL && m_strSSLPem.isEmpty())) {
          Logger::global()->enterLogMessage(
              "HttpService",
              tr("Restarting the service due to a port or SSL config change"));

          killCaptchaClients();

          m_server->stop();
          delete m_server;
          setup();
        } else
          setupAuth();
      }
    } else if (!enable && m_server) {
      killCaptchaClients();
      m_server->stop();
      delete m_server;
      m_server = 0;
    }
  } catch (const RuntimeException& e) {
    qDebug() << e.what();
    Logger::global()->enterLogMessage("HttpService", e.what());
  }
}

void HttpService::setupAuth() {
  QString password = getSettingsValue("remote/password").toString();

  AuthenticatedRequestHandler::setPassword(password);
}

void HttpService::setup() {
  m_port = getSettingsValue("remote/port").toUInt();

  try {
    HTTPServerParams* params = new HTTPServerParams;

    // if not SSL
    setupAuth();
    setupSSL();

    m_server = new HTTPServer(m_instance, *m_socket, params);
    m_server->start();

    Logger::global()->enterLogMessage("HttpService",
                                      tr("Listening on port %1").arg(m_port));
    std::cout << "Listening on port " << m_port << std::endl;
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    Logger::global()->enterLogMessage("HttpService",
                                      tr("Failed to start: %1").arg(e.what()));
  }
}

void HttpService::setupSSL() {
  m_bUseSSL = getSettingsValue("remote/ssl").toBool();

  if (m_bUseSSL) {
    QString file = getSettingsValue("remote/ssl_pem").toString();
    if (file.isEmpty() || !QFile::exists(file)) {
      Logger::global()->enterLogMessage(
          "HttpService", tr("SSL key file not found, disabling HTTPS"));
      m_socket.reset(new ServerSocket(m_port));
      m_strSSLPem.clear();
    } else {
      Logger::global()->enterLogMessage(
          "HttpService", tr("Loading a SSL key from %1").arg(file));
      m_strSSLPem = file;

      Context* context =
          new Context(Context::Usage::SERVER_USE, file.toStdString(),
                      std::string(), std::string());
      m_socket.reset(new SecureServerSocket(m_port, 5, context));
    }
  } else {
    Logger::global()->enterLogMessage("HttpService",
                                      tr("Running in plain HTTP mode"));
    m_socket.reset(new ServerSocket(m_port));
  }
}

HTTPRequestHandler* HttpService::createRequestHandler(
    const HTTPServerRequest& request) {
  QString uri = QString::fromStdString(request.getURI());

  m_handlersMutex.lock();

  for (const QPair<QRegExp, handler_t>& e : m_handlers) {
    if (e.first.exactMatch(uri)) {
      m_handlersMutex.unlock();
      return e.second();
    }
  }

  m_handlersMutex.unlock();

  return new FileRequestHandler("/", DATA_LOCATION "/data/remote/");
}

HttpService::LogService::LogService(const QString& mapping)
    : m_mapping(mapping) {}

void HttpService::LogService::run() {
  QString data, uuidTransfer;
  QString uri = QString::fromStdString(request().getURI());

  uuidTransfer = uri.mid(m_mapping.length() + 1);

  if (uuidTransfer.isEmpty())
    data = Logger::global()->logContents();
  else {
    Queue* q = 0;
    Transfer* t = 0;

    uuidTransfer = QUrl::fromPercentEncoding(uuidTransfer.toLatin1());
    // qDebug() << "Transfer:" << uuidTransfer;

    findTransfer(uuidTransfer, &q, &t);

    if (!q || !t) {
      this->sendErrorResponse(
          Poco::Net::HTTPResponse::HTTPStatus::HTTP_NOT_FOUND,
          "Queue/Transfer not found");
      return;
    }

    data = t->logContents();

    q->unlock();
    g_queuesLock.unlock();
  }

  QByteArray ba = data.toUtf8();
  response().setContentType("text/plain");
  response().setContentLength(ba.size());
  response().sendBuffer(ba.data(), ba.size());
}

HttpService::TransferTreeBrowserService::TransferTreeBrowserService(
    const QString& mapping)
    : m_mapping(mapping) {}

void HttpService::TransferTreeBrowserService::run() {
  QUrl url(QString::fromStdString(request().getURI()));
  QUrlQuery query(url);

  QString uuidTransfer = url.path().mid(m_mapping.length() + 1);  // "/browse/"
  QString path = query.queryItemValue("path");

  uuidTransfer = QUrl::fromPercentEncoding(uuidTransfer.toLatin1());

  if (uuidTransfer.startsWith("/")) uuidTransfer = uuidTransfer.mid(1);

  Queue* q = 0;
  Transfer* t = 0;

  if (path.contains("/..") || path.contains("../")) {
    sendErrorResponse(Poco::Net::HTTPResponse::HTTPStatus::HTTP_FORBIDDEN,
                      ".. in path");
    return;
  }

  findTransfer(uuidTransfer, &q, &t);

  if (!q || !t) {
    sendErrorResponse(Poco::Net::HTTPResponse::HTTPStatus::HTTP_NOT_FOUND,
                      "Transfer UUID not found");
    return;
  }

  QDomDocument doc;
  QDomElement root, item;
  QFileInfoList infoList;
  QDir dir(t->dataPath());

  if (path.startsWith("/")) path = path.mid(1);

  if (!dir.cd(path)) {
    sendErrorResponse(Poco::Net::HTTPResponse::HTTPStatus::HTTP_NOT_FOUND,
                      "Not found");
    return;
  }

  root = doc.createElement("root");
  doc.appendChild(root);

  infoList = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
  foreach (QFileInfo info, infoList) {
    item = doc.createElement("item");
    item.setAttribute("id", path + "/" + info.fileName());
    item.setAttribute("state", "closed");
    item.setAttribute("type", "folder");

    QDomElement content, name;
    content = doc.createElement("content");
    name = doc.createElement("name");

    name.appendChild(doc.createTextNode(info.fileName()));
    content.appendChild(name);
    item.appendChild(content);
    root.appendChild(item);
  }

  infoList = dir.entryInfoList(QDir::Files);
  foreach (QFileInfo info, infoList) {
    item = doc.createElement("item");
    item.setAttribute("id", path + "/" + info.fileName());
    item.setAttribute("type", "default");

    QDomElement content, name;
    content = doc.createElement("content");
    name = doc.createElement("name");

    name.appendChild(doc.createTextNode(info.fileName()));
    content.appendChild(name);
    item.appendChild(content);
    root.appendChild(item);
  }

  q->unlock();
  g_queuesLock.unlock();

  QByteArray ba = doc.toString(0).toUtf8();

  response().setContentType("text/xml");
  response().sendBuffer(ba.data(), ba.size());
}

HttpService::TransferDownloadService::TransferDownloadService(
    const QString& mapping)
    : m_mapping(mapping) {}

void HttpService::TransferDownloadService::run() {
  QUrl url(QString::fromStdString(request().getURI()));
  QUrlQuery query(url);

  QString transfer = query.queryItemValue("transfer");
  QString path = query.queryItemValue("path");

  Queue* q = 0;
  Transfer* t = 0;

  if (path.contains("/..") || path.contains("../")) {
    sendErrorResponse(Poco::Net::HTTPResponse::HTTPStatus::HTTP_FORBIDDEN,
                      ".. in path");
    return;
  }

  findTransfer(transfer, &q, &t);

  if (!q || !t) {
    sendErrorResponse(Poco::Net::HTTPResponse::HTTPStatus::HTTP_NOT_FOUND,
                      "Transfer UUID not found");
    return;
  }

  if (!path.startsWith("/")) path.prepend("/");
  if (path.endsWith("/")) path = path.left(path.size() - 1);
  path.prepend(t->dataPath(true));

  q->unlock();
  g_queuesLock.unlock();

  QString disposition;
  int last = path.lastIndexOf('/');
  if (last > 0) {
    disposition = path.mid(last + 1);
    if (disposition.size() > 100) disposition.resize(100);
  }

  disposition = QString("attachment; filename=\"%1\"").arg(disposition);

  response().add("Content-Disposition", disposition.toStdString());
  response().sendFile(path.toStdString(), "application/octet-stream");
}

HttpService::SubclassService::SubclassService(const QString& mapping)
    : m_mapping(mapping) {}

void HttpService::SubclassService::run() {
  QUrl url(QString::fromStdString(request().getURI()));
  QUrlQuery query(url);

  HttpService::WriteBackImpl wb = HttpService::WriteBackImpl(response());
  QString transfer = query.queryItemValue("transfer");
  QString method = url.path().mid(m_mapping.length() + 1);  // "/subclass/"

  Queue* q = 0;
  Transfer* t = 0;

  findTransfer(transfer, &q, &t);

  if (!q || !t || !dynamic_cast<TransferHttpService*>(t)) {
    if (t) {
      q->unlock();
      g_queuesLock.unlock();
    }

    sendErrorResponse(Poco::Net::HTTPResponse::HTTPStatus::HTTP_NOT_FOUND,
                      "Transfer UUID not found");
    return;
  }

  QMultiMap<QString, QString> map;
  QList<QPair<QString, QString> > items = query.queryItems();

  for (const QPair<QString, QString>& item : items)
    map.insert(item.first, item.second);

  TransferHttpService* s = dynamic_cast<TransferHttpService*>(t);

  if (s != nullptr) s->process(method, map, &wb);

  q->unlock();
  g_queuesLock.unlock();
}

HttpService::WriteBackImpl::WriteBackImpl(Poco::Net::HTTPServerResponse& resp)
    : m_response(resp) {}

void HttpService::WriteBackImpl::sendHeaders() {
  if (m_ostream) return;

  m_response.setChunkedTransferEncoding(true);
  m_response.setStatusAndReason(
      Poco::Net::HTTPServerResponse::HTTPStatus::HTTP_OK);
  m_ostream = &m_response.send();
}

void HttpService::WriteBackImpl::write(const char* data, size_t bytes) {
  sendHeaders();
  m_ostream->write(static_cast<const char*>(data), bytes);
}

void HttpService::WriteBackImpl::writeFail(QString error) {
  QByteArray ba = error.toUtf8();
  m_response.setStatusAndReason(
      Poco::Net::HTTPServerResponse::HTTPStatus::HTTP_INTERNAL_SERVER_ERROR);
  m_response.sendBuffer(ba.data(), ba.size());
}

void HttpService::WriteBackImpl::writeNoCopy(void* data, size_t bytes) {
  sendHeaders();
  m_ostream->write(static_cast<const char*>(data), bytes);
}

void HttpService::WriteBackImpl::send() {
  // No-op with poco
}

void HttpService::WriteBackImpl::setContentType(const char* type) {
  m_response.setContentType(type);
}

int HttpService::findTransfer(QString transferUUID, Queue** q, Transfer** t,
                              bool lockForWrite) {
  *q = 0;
  *t = 0;

  g_queuesLock.lockForRead();
  for (int i = 0; i < g_queues.size(); i++) {
    Queue* c = g_queues[i];
    if (lockForWrite)
      c->lockW();
    else
      c->lock();

    for (int j = 0; j < c->size(); j++) {
      if (c->at(j)->uuid() == transferUUID) {
        *q = c;
        *t = c->at(j);
        return j;
      }
    }

    c->unlock();
  }

  g_queuesLock.unlock();
  return -1;
}

void HttpService::findQueue(QString queueUUID, Queue** q) {
  *q = 0;

  QReadLocker r(&g_queuesLock);
  for (int i = 0; i < g_queues.size(); i++) {
    Queue* c = g_queues[i];

    if (c->uuid() == queueUUID) {
      *q = c;
      return;
    }
  }
}

QVariant HttpService::generateCertificate(QList<QVariant>& args) {
  QString hostname = args[0].toString();
  const char* script = DATA_LOCATION "/data/genssl.sh";
  const char* config = DATA_LOCATION "/data/genssl.cnf";
  const char* pemfile = "/tmp/fatrat-webui.pem";

  QProcess prc;

  qDebug() << "Starting: " << script << " " << config;
  prc.start(script, QStringList() << config << hostname);
  prc.waitForFinished();

  if (prc.exitCode() != 0)
    throw XmlRpcService::XmlRpcError(
        501, tr("Failed to generate a certificate, please ensure you have "
                "'openssl' and 'sed' installed."));

  QFile file(pemfile);
  if (!file.open(QIODevice::ReadOnly))
    throw XmlRpcService::XmlRpcError(
        501, tr("Failed to generate a certificate, please ensure you have "
                "'openssl' and 'sed' installed."));

  QByteArray data = file.readAll();
  QDir::home().mkpath(USER_PROFILE_PATH "/data");

  QString path = QDir::homePath() + QLatin1String(USER_PROFILE_PATH) +
                 "/data/fatrat-webui.pem";
  QFile out(path);

  if (!out.open(QIODevice::WriteOnly))
    throw XmlRpcService::XmlRpcError(
        502, tr("Failed to open %1 for writing.").arg(path));

  out.write(data);

  out.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
  out.close();
  file.remove();

  return path;
}

void HttpService::CaptchaService::run() {
  QString solution;
  int id;

  if (!form().has("id") || !form().has("solution")) {
    sendErrorResponse(Poco::Net::HTTPResponse::HTTPStatus::HTTP_BAD_REQUEST,
                      "id or solution missing");
    return;
  }

  id = std::stoi(form().get("id"));
  solution = QString::fromStdString(form().get("solution"));

  HttpService::instance()->m_captchaHttp.captchaEntered(id, solution);

  response().setStatusAndReason(
      Poco::Net::HTTPServerResponse::HTTPStatus::HTTP_NO_CONTENT);
  response().setContentLength(0);
  response().send();
}

HttpService::WebSocketService::WebSocketService() {
  if (::pipe(m_pipe) != 0) {
    m_pipe[0] = m_pipe[1] = -1;
    throw std::runtime_error("Cannot create a wakeup pipe");
  }
}

HttpService::WebSocketService::~WebSocketService() {
  close(m_pipe[0]);
  close(m_pipe[1]);
}

class WebSocketPublicFD : public WebSocket {
 public:
  WebSocketPublicFD(HTTPServerRequest& request, HTTPServerResponse& response)
      : WebSocket(request, response) {}

  poco_socket_t sockfd() const { return WebSocket::sockfd(); }
};

void HttpService::WebSocketService::run() {
  WebSocketPublicFD ws(request(), response());
  const int fdWs = ws.sockfd();
  bool stop = false;
  struct pollfd fds[2] = {{fdWs, POLLIN, 0}, {m_pipe[0], POLLIN, 0}};

  HttpService::instance()->addCaptchaClient(this);

  try {
    while (!stop) {
      int ev = ::poll(fds, sizeof(fds) / sizeof(fds[0]), 500);

      if (ev) {
        if (fds[0].revents) {
          // Data from client
          int flags, len;
          char buf[32];

          len = ws.receiveFrame(buf, sizeof(buf), flags);

          if (!len) break;  // Connection closed

          if ((flags & WebSocket::FRAME_OP_BITMASK) == WebSocket::FRAME_OP_PING)
            ws.sendFrame(nullptr, 0, WebSocket::FRAME_OP_PONG);
          else if ((flags & WebSocket::FRAME_OP_BITMASK) ==
                   WebSocket::FRAME_OP_CLOSE)
            break;
        }
        if (fds[1].revents) {
          int v;

          read(m_pipe[0], &v, sizeof(v));

          switch (v) {
            case -1:
              stop = true;
              break;
            case 0:
              // new captcha to be solved
              pushMore(ws);
              break;
            case 1: {
              // pseudo-ping; real ping frames were failing for me in browsers
              const char* text = "{\"type\": \"ping\"}";
              ws.sendFrame(text, strlen(text));
              break;
            }
          }
        }
      }
    }
  } catch (...) {
  }

  if (!stop) {
    HttpService* service = HttpService::instance();
    if (service)
      service->removeCaptchaClient(
          this);  // only remove self unless we were told to terminate
  }
}

void HttpService::WebSocketService::pushMore(WebSocket& ws) {
  captchaQueueLock.lock();

  while (!captchaQueue.isEmpty()) {
    QJsonObject notif;
    Captcha cap = captchaQueue.dequeue();
    QByteArray ba;

    notif.insert("type", "captcha");
    notif.insert("id", cap.first);
    notif.insert("url", cap.second);

    ba = QJsonDocument(notif).toJson(QJsonDocument::JsonFormat::Compact);
    ws.sendFrame(ba.data(), ba.size());
  }

  captchaQueueLock.unlock();
}

void HttpService::WebSocketService::pushCaptcha(const Captcha& cap) {
  qDebug() << "Enqueueing captcha id" << cap.first;
  captchaQueueLock.lock();
  captchaQueue << cap;
  captchaQueueLock.unlock();
  wakeup(0);
}

void HttpService::WebSocketService::wakeup(int v) {
  ::write(m_pipe[1], &v, sizeof(v));
}

void HttpService::addCaptchaEvent(int id, QString url) {
  QMutexLocker l(&m_registeredCaptchaClientsMutex);

  for (WebSocketService* ws : m_registeredCaptchaClients) {
    ws->pushCaptcha(WebSocketService::Captcha(id, url));
  }
}

void HttpService::addCaptchaClient(WebSocketService* client) {
  QMutexLocker l(&m_registeredCaptchaClientsMutex);
  m_registeredCaptchaClients << client;
}

void HttpService::removeCaptchaClient(WebSocketService* client) {
  QMutexLocker l(&m_registeredCaptchaClientsMutex);
  m_registeredCaptchaClients.removeAll(client);
}

bool HttpService::hasCaptchaHandlers() {
  return !m_registeredCaptchaClients.isEmpty();
}

void HttpService::keepalive() {
  QMutexLocker l(&m_registeredCaptchaClientsMutex);

  for (WebSocketService* cl : m_registeredCaptchaClients) cl->keepalive();
}
