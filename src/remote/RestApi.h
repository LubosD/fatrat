/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2026 Lubos Dolezel <lubos a dolezel.info>

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


#ifndef RESTAPI_H
#define RESTAPI_H
#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QMultiMap>
#include <QString>
#include <QStringList>
#include <functional>
#include <optional>

// A failure to be reported to the client as {"error": {"code": ..., "message": ...}}
struct RestError
{
	RestError(int status, QString code, QString message)
		: status(status), code(code), message(message) {}

	int status;
	QString code;
	QString message;
};

struct RestUpload
{
	QString filename;
	QString contentType;
	QByteArray data;
};

// Transport-independent view of an /api/v1 request (filled in by HttpService, or by the tests)
struct RestRequest
{
	QString method; // upper case
	QString path; // the full path, including /api/v1
	QMap<QString,QString> pathParams;
	QMultiMap<QString,QString> query;
	QString contentType;
	QString origin;
	QString host;
	QByteArray body;
	QMap<QString,QString> formFields;
	QMap<QString,RestUpload> formFiles;

	bool isMultipart() const;
	// The body as JSON; throws RestError(400, "invalid_json")
	QJsonValue json() const;
	QJsonObject jsonObject() const;
	QStringList queryValues(const QString& name) const;
	bool queryBool(const QString& name) const;
private:
	mutable std::optional<QJsonValue> m_json;
};

struct RestResult
{
	int status = 200;
	QJsonValue body = QJsonValue::Undefined; // Undefined: no body
	QString location;

	static RestResult ok(QJsonValue body) { RestResult r; r.body = body; return r; }
	static RestResult created(QJsonValue body, QString location) { RestResult r; r.status = 201; r.body = body; r.location = location; return r; }
	static RestResult noContent() { RestResult r; r.status = 204; return r; }
	static RestResult accepted() { RestResult r; r.status = 202; return r; }
};

struct RestResponse
{
	int status = 200;
	QByteArray body;
	QString contentType;
	QMap<QString,QString> headers;
};

typedef std::function<RestResult(const RestRequest&)> RestHandler;

namespace RestApi
{
	extern const char* const Prefix; // "/api/v1"
	bool isApiPath(const QString& path);

	// pattern is relative to Prefix, path parameters start with a colon: "/queues/:queue/transfers"
	void registerRoute(const QString& method, const QString& pattern, RestHandler handler, bool allowMultipart = false);
	void clearRoutes();

	// Applies the CSRF guard, routes the request, runs the handler and maps every failure to an error response.
	// Authentication is not done here.
	RestResponse dispatch(RestRequest& req);

	// Validation helpers for request bodies. All of them throw RestError(422, "invalid_property", ...).
	QString asString(const QJsonValue& v, const QString& field);
	bool asBool(const QJsonValue& v, const QString& field);
	int asInt(const QJsonValue& v, const QString& field);
	QStringList asStringList(const QJsonValue& v, const QString& field);
	void asSpeedPair(const QJsonValue& v, const QString& field, int& down, int& up);

	QJsonObject speedPairToJson(int down, int up);
	QJsonValue variantToJson(const QVariant& v);
	QVariant jsonToVariant(const QJsonValue& v);
}

#endif
