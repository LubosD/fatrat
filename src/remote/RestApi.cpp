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


#include "RestApi.h"
#include "RuntimeException.h"
#include <QJsonDocument>
#include <QUrl>
#include <QVector>
#include <cmath>

const char* const RestApi::Prefix = "/api/v1";

namespace
{
	struct Route
	{
		QString method;
		QStringList segments;
		RestHandler handler;
		bool allowMultipart;
	};

	QVector<Route>& routes()
	{
		static QVector<Route> r;
		return r;
	}

	QStringList splitPath(const QString& path)
	{
		return path.split('/', Qt::SkipEmptyParts);
	}

	bool matchRoute(const Route& r, const QStringList& segs, QMap<QString,QString>& params)
	{
		if (r.segments.size() != segs.size())
			return false;

		params.clear();
		for (int i = 0; i < segs.size(); i++)
		{
			if (r.segments[i].startsWith(':'))
				params[r.segments[i].mid(1)] = QUrl::fromPercentEncoding(segs[i].toUtf8());
			else if (r.segments[i] != segs[i])
				return false;
		}
		return true;
	}

	RestResponse errorResponse(int status, const QString& code, const QString& message)
	{
		QJsonObject err;
		err["code"] = code;
		err["message"] = message;
		QJsonObject root;
		root["error"] = err;

		RestResponse rv;
		rv.status = status;
		rv.contentType = "application/json";
		rv.body = QJsonDocument(root).toJson(QJsonDocument::Compact);
		return rv;
	}

	// Browsers cache HTTP Basic credentials and send them on cross-site requests, so state-changing
	// requests must be provably same-origin or require a CORS preflight (which is never granted).
	void checkGuard(const RestRequest& req, const Route* route)
	{
		if (!req.origin.isEmpty())
		{
			QString originHost = QUrl(req.origin).authority();
			if (originHost.isEmpty() || originHost.compare(req.host, Qt::CaseInsensitive) != 0)
				throw RestError(403, "forbidden_origin", "Cross-origin requests are not allowed");
		}

		if (req.method == "GET" || req.method == "HEAD" || req.body.isEmpty())
			return;

		const QString type = req.contentType.section(';', 0, 0).trimmed().toLower();
		if (type == "application/json")
			return;
		if (type == "multipart/form-data" && route && route->allowMultipart)
			return;

		throw RestError(415, "unsupported_media_type", "Content-Type must be application/json");
	}
}

bool RestApi::isApiPath(const QString& path)
{
	return path == Prefix || path.startsWith(QString(Prefix) + '/');
}

void RestApi::registerRoute(const QString& method, const QString& pattern, RestHandler handler, bool allowMultipart)
{
	Route r;
	r.method = method;
	r.segments = splitPath(pattern);
	r.handler = handler;
	r.allowMultipart = allowMultipart;
	routes() << r;
}

void RestApi::clearRoutes()
{
	routes().clear();
}

RestResponse RestApi::dispatch(RestRequest& req)
{
	try
	{
		QStringList segs = splitPath(req.path);
		int prefixSegs = splitPath(Prefix).size();
		if (segs.size() < prefixSegs)
			throw RestError(404, "unknown_route", "No such resource");
		segs = segs.mid(prefixSegs);

		const Route* found = nullptr;
		QStringList allowed;
		for (const Route& r : routes())
		{
			QMap<QString,QString> params;
			if (!matchRoute(r, segs, params))
				continue;

			if (r.method == req.method)
			{
				found = &r;
				req.pathParams = params;
				break;
			}
			allowed << r.method;
		}

		checkGuard(req, found);

		if (!found)
		{
			if (allowed.isEmpty())
				throw RestError(404, "unknown_route", "No such resource");

			RestResponse rv = errorResponse(405, "method_not_allowed", "Method not allowed for this resource");
			rv.headers["Allow"] = allowed.join(", ");
			return rv;
		}

		RestResult result = found->handler(req);

		RestResponse rv;
		rv.status = result.status;
		if (!result.location.isEmpty())
			rv.headers["Location"] = result.location;
		if (!result.body.isUndefined())
		{
			rv.contentType = "application/json";
			if (result.body.isArray())
				rv.body = QJsonDocument(result.body.toArray()).toJson(QJsonDocument::Compact);
			else if (result.body.isObject())
				rv.body = QJsonDocument(result.body.toObject()).toJson(QJsonDocument::Compact);
			else
				rv.body = QJsonDocument(QJsonArray() << result.body).toJson(QJsonDocument::Compact).mid(1).chopped(1);
		}
		return rv;
	}
	catch (const RestError& e)
	{
		return errorResponse(e.status, e.code, e.message);
	}
	catch (const RuntimeException& e)
	{
		return errorResponse(500, "internal_error", e.what());
	}
	catch (const std::exception& e)
	{
		return errorResponse(500, "internal_error", QString::fromUtf8(e.what()));
	}
}

bool RestRequest::isMultipart() const
{
	return contentType.section(';', 0, 0).trimmed().compare("multipart/form-data", Qt::CaseInsensitive) == 0;
}

QJsonValue RestRequest::json() const
{
	if (!m_json)
	{
		QJsonParseError err;
		QJsonDocument doc = QJsonDocument::fromJson(body, &err);
		if (err.error != QJsonParseError::NoError)
			throw RestError(400, "invalid_json", QString("Invalid JSON: %1").arg(err.errorString()));

		m_json = doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object());
	}
	return *m_json;
}

QJsonObject RestRequest::jsonObject() const
{
	QJsonValue v = json();
	if (!v.isObject())
		throw RestError(400, "invalid_json", "A JSON object was expected");
	return v.toObject();
}

QStringList RestRequest::queryValues(const QString& name) const
{
	return query.values(name);
}

bool RestRequest::queryBool(const QString& name) const
{
	const QString v = query.value(name).toLower();
	return v == "true" || v == "1";
}

static RestError invalid(const QString& field, const QString& expected)
{
	return RestError(422, "invalid_property", QString("Property \"%1\" must be %2").arg(field, expected));
}

QString RestApi::asString(const QJsonValue& v, const QString& field)
{
	if (!v.isString())
		throw invalid(field, "a string");
	return v.toString();
}

bool RestApi::asBool(const QJsonValue& v, const QString& field)
{
	if (!v.isBool())
		throw invalid(field, "a boolean");
	return v.toBool();
}

int RestApi::asInt(const QJsonValue& v, const QString& field)
{
	if (!v.isDouble())
		throw invalid(field, "an integer");

	double d = v.toDouble();
	if (d != std::floor(d) || d < -2147483648.0 || d > 2147483647.0)
		throw invalid(field, "an integer");
	return int(d);
}

QStringList RestApi::asStringList(const QJsonValue& v, const QString& field)
{
	if (!v.isArray())
		throw invalid(field, "an array of strings");

	QStringList rv;
	for (const QJsonValue& e : v.toArray())
	{
		if (!e.isString())
			throw invalid(field, "an array of strings");
		rv << e.toString();
	}
	return rv;
}

void RestApi::asSpeedPair(const QJsonValue& v, const QString& field, int& down, int& up)
{
	if (!v.isObject())
		throw invalid(field, "an object with integer \"down\" and \"up\"");

	QJsonObject o = v.toObject();
	if (o.size() != 2 || !o.contains("down") || !o.contains("up"))
		throw invalid(field, "an object with integer \"down\" and \"up\"");

	down = asInt(o["down"], field + ".down");
	up = asInt(o["up"], field + ".up");
}

QJsonObject RestApi::speedPairToJson(int down, int up)
{
	QJsonObject o;
	o["down"] = down;
	o["up"] = up;
	return o;
}

QJsonValue RestApi::variantToJson(const QVariant& v)
{
	if (!v.isValid() || v.isNull())
		return QJsonValue::Null;
	return QJsonValue::fromVariant(v);
}

QVariant RestApi::jsonToVariant(const QJsonValue& v)
{
	// JSON has one number type, which QJsonValue hands out as double. Settings code expects real integers.
	if (v.isDouble())
	{
		double d = v.toDouble();
		if (d == std::floor(d) && std::fabs(d) < 9007199254740992.0)
		{
			if (d >= -2147483648.0 && d <= 2147483647.0)
				return QVariant(int(d));
			return QVariant(qlonglong(d));
		}
	}
	return v.toVariant();
}
