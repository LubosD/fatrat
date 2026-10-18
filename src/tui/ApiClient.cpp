#include "ApiClient.h"
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QUuid>

namespace tui
{

QString ApiResult::describe() const
{
	if (transportFailure())
		return errorMessage;
	if (!errorMessage.isEmpty())
		return errorMessage;
	return QString("HTTP %1").arg(status);
}

QJsonObject AddRequest::toJson(bool withUrls) const
{
	QJsonObject o;
	if (withUrls)
		o["urls"] = QJsonArray::fromStringList(urls);
	o["upload"] = upload;
	o["class"] = cls;
	o["target"] = target;
	o["paused"] = paused;
	o["speedLimits"] = QJsonObject{ { "down", double(downLimit) }, { "up", double(upLimit) } };
	return o;
}

ApiClient::ApiClient(HttpClient* http, QObject* parent) : QObject(parent), m_http(http)
{
}

QString ApiClient::encode(const QString& s)
{
	return QString::fromLatin1(QUrl::toPercentEncoding(s));
}

static ApiResult toResult(const HttpResponse& r)
{
	ApiResult a;
	a.status = r.status;
	a.raw = r.body;
	if (r.status == 0)
	{
		a.errorMessage = r.error;
		return a;
	}
	if (!r.body.isEmpty() && r.headers.value("content-type").startsWith("application/json"))
	{
		QJsonParseError err;
		// the API returns scalars too; wrap in an array to parse them
		QJsonDocument doc = QJsonDocument::fromJson(r.body, &err);
		if (err.error == QJsonParseError::NoError)
			a.json = doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object());
	}
	if (!r.ok())
	{
		QJsonObject e = a.json.toObject()["error"].toObject();
		a.errorCode = e["code"].toString();
		a.errorMessage = e["message"].toString();
		if (a.errorMessage.isEmpty())
			a.errorMessage = r.status == 401 ? QStringLiteral("Wrong password") : QString("HTTP %1").arg(r.status);
	}
	return a;
}

void ApiClient::call(const QByteArray& method, const QString& path, const QJsonValue& body, Callback cb)
{
	QByteArray payload;
	if (body.isObject())
		payload = QJsonDocument(body.toObject()).toJson(QJsonDocument::Compact);
	else if (body.isArray())
		payload = QJsonDocument(body.toArray()).toJson(QJsonDocument::Compact);

	m_http->request(method, "/api/v1" + path, [cb](const HttpResponse& r) {
		if (cb)
			cb(toResult(r));
	}, payload);
}

void ApiClient::stats(Callback cb) { get("/stats", std::move(cb)); }
void ApiClient::queues(Callback cb) { get("/queues", std::move(cb)); }
void ApiClient::queueTransfers(const QString& q, Callback cb) { get("/queues/" + encode(q) + "/transfers", std::move(cb)); }
void ApiClient::transferClasses(Callback cb) { get("/transfer-classes", std::move(cb)); }
void ApiClient::transferDetails(const QString& t, Callback cb) { get("/transfers/" + encode(t) + "/details", std::move(cb)); }

void ApiClient::transferOptions(const QString& t, Callback cb) { get("/transfers/" + encode(t) + "/options", std::move(cb)); }

void ApiClient::patchTransferOptions(const QString& t, const QJsonObject& values, Callback cb)
{
	call("PATCH", "/transfers/" + encode(t) + "/options", values, std::move(cb));
}

void ApiClient::listDirectories(const QString& path, const QString& queue, bool showHidden, Callback cb)
{
	QString p = "/filesystem/directories?showHidden=" + QString(showHidden ? "true" : "false");
	if (!path.isEmpty())
		p += "&path=" + encode(path);
	if (!queue.isEmpty())
		p += "&queue=" + encode(queue);
	get(p, std::move(cb));
}

void ApiClient::createQueue(const QJsonObject& props, Callback cb) { call("POST", "/queues", props, std::move(cb)); }
void ApiClient::patchQueue(const QString& q, const QJsonObject& props, Callback cb) { call("PATCH", "/queues/" + encode(q), props, std::move(cb)); }

void ApiClient::deleteQueue(const QString& q, const DeleteQueueOptions& o, Callback cb)
{
	QString p = "/queues/" + encode(q);
	QStringList qs;
	if (!o.moveTo.isEmpty())
		qs << "moveTo=" + encode(o.moveTo);
	if (o.deleteTransfers)
		qs << "deleteTransfers=true";
	if (o.withData)
		qs << "withData=true";
	if (!qs.isEmpty())
		p += "?" + qs.join('&');
	call("DELETE", p, QJsonValue(), std::move(cb));
}

void ApiClient::moveQueue(const QString& q, const QString& dir, Callback cb)
{
	call("POST", "/queues/move", QJsonObject{ { "uuid", q }, { "direction", dir } }, std::move(cb));
}

void ApiClient::addTransfers(const QString& q, const AddRequest& r, Callback cb)
{
	call("POST", "/queues/" + encode(q) + "/transfers", r.toJson(), std::move(cb));
}

void ApiClient::uploadTransfer(const QString& q, const QString& filePath, const AddRequest& r, Callback cb)
{
	QFile f(filePath);
	if (!f.open(QIODevice::ReadOnly))
	{
		ApiResult res;
		res.errorMessage = QString("Cannot read %1: %2").arg(filePath, f.errorString());
		if (cb)
			cb(res);
		return;
	}

	const QByteArray boundary = "----fatrat-tui-" + QUuid::createUuid().toByteArray(QUuid::WithoutBraces);
	QByteArray body;
	body += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"options\"\r\n\r\n";
	body += QJsonDocument(r.toJson(false)).toJson(QJsonDocument::Compact) + "\r\n";
	body += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"";
	body += QFileInfo(filePath).fileName().replace('"', '_').toUtf8();
	body += "\"\r\nContent-Type: application/octet-stream\r\n\r\n";
	body += f.readAll();
	body += "\r\n--" + boundary + "--\r\n";

	HttpRequest req;
	req.method = "POST";
	req.path = "/api/v1/queues/" + encode(q) + "/transfers";
	req.body = body;
	req.contentType = "multipart/form-data; boundary=" + boundary;
	m_http->request(req, [cb](const HttpResponse& r) { if (cb) cb(toResult(r)); });
}

void ApiClient::patchTransfers(const QStringList& uuids, const QJsonObject& props, Callback cb)
{
	call("PATCH", "/transfers", QJsonObject{ { "uuids", QJsonArray::fromStringList(uuids) }, { "properties", props } }, std::move(cb));
}

void ApiClient::patchTransfer(const QString& uuid, const QJsonObject& props, Callback cb)
{
	call("PATCH", "/transfers/" + encode(uuid), props, std::move(cb));
}

void ApiClient::deleteTransfers(const QStringList& uuids, bool withData, Callback cb)
{
	// the uuids are in the query string: send long selections in several requests
	const int batch = 40;
	auto pending = std::make_shared<int>((uuids.size() + batch - 1) / batch);
	auto first = std::make_shared<ApiResult>();
	auto failed = std::make_shared<bool>(false);
	if (*pending == 0)
	{
		if (cb)
			cb(ApiResult{});
		return;
	}
	for (int i = 0; i < uuids.size(); i += batch)
	{
		QStringList qs;
		for (const QString& u : uuids.mid(i, batch))
			qs << "uuid=" + encode(u);
		qs << QString("withData=") + (withData ? "true" : "false");
		call("DELETE", "/transfers?" + qs.join('&'), QJsonValue(), [=](const ApiResult& r) {
			if (!r.ok() && !*failed)
			{
				*failed = true;
				*first = r;
			}
			if (--*pending == 0 && cb)
			{
				if (*failed)
					cb(*first);
				else
				{
					ApiResult ok;
					ok.status = 204;
					cb(ok);
				}
			}
		});
	}
}

void ApiClient::moveTransfers(const QString& q, const QStringList& uuids, const QString& dir, Callback cb)
{
	call("POST", "/queues/" + encode(q) + "/transfers/move",
		QJsonObject{ { "uuids", QJsonArray::fromStringList(uuids) }, { "direction", dir } }, std::move(cb));
}

void ApiClient::moveTransfersBefore(const QString& q, const QStringList& uuids, const QString& before, Callback cb)
{
	call("POST", "/queues/" + encode(q) + "/transfers/move",
		QJsonObject{ { "uuids", QJsonArray::fromStringList(uuids) }, { "before", before.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(before) } }, std::move(cb));
}

void ApiClient::setFilePriorities(const QString& t, const QJsonArray& items, Callback cb)
{
	call("PUT", "/transfers/" + encode(t) + "/torrent/file-priorities", items, std::move(cb));
}

// The log routes live outside /api/v1
void ApiClient::globalLog(Callback cb)
{
	m_http->request("GET", "/log", [cb](const HttpResponse& r) { if (cb) cb(toResult(r)); });
}

void ApiClient::transferLog(const QString& t, Callback cb)
{
	m_http->request("GET", "/log/" + encode(t), [cb](const HttpResponse& r) { if (cb) cb(toResult(r)); });
}

void ApiClient::settingsPages(Callback cb) { get("/settings/pages", std::move(cb)); }

void ApiClient::schedulerActions(Callback cb) { get("/scheduler/actions", std::move(cb)); }
void ApiClient::createSchedulerAction(const QJsonObject& props, Callback cb) { call("POST", "/scheduler/actions", props, std::move(cb)); }
void ApiClient::patchSchedulerAction(const QString& id, const QJsonObject& props, Callback cb)
{
	call("PATCH", "/scheduler/actions/" + encode(id), props, std::move(cb));
}
void ApiClient::deleteSchedulerAction(const QString& id, Callback cb) { call("DELETE", "/scheduler/actions/" + encode(id), QJsonValue(), std::move(cb)); }

void ApiClient::settingsValues(const QStringList& keys, Callback cb)
{
	QStringList qs;
	for (const QString& k : keys)
		qs << "key=" + encode(k);
	get("/settings?" + qs.join('&'), std::move(cb));
}

void ApiClient::patchSettings(const QJsonObject& values, Callback cb) { call("PATCH", "/settings", values, std::move(cb)); }
void ApiClient::applySettings(Callback cb) { call("POST", "/settings/apply", QJsonObject(), std::move(cb)); }

void ApiClient::generateCertificate(const QString& hostname, Callback cb)
{
	call("POST", "/settings/web-interface/certificate", QJsonObject{ { "hostname", hostname } }, std::move(cb));
}

void ApiClient::rssFeeds(Callback cb) { get("/rss/feeds", std::move(cb)); }
void ApiClient::rssRegexps(Callback cb) { get("/rss/regexps", std::move(cb)); }
void ApiClient::rssTest(const QJsonObject& body, Callback cb) { call("POST", "/rss/test", body, std::move(cb)); }

}
