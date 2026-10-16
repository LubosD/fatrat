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

#include "PyPython.h"

#include "PyPlugin.h"

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrlQuery>
#include <QtDebug>

#include "Logger.h"
#include "fatrat.h"

namespace py = pybind11;

// Accept any mapping where a dict is wanted (plugins pass dicts, but also
// whatever their parser handed them).
static py::dict asDict(py::handle mapping)
{
	if (py::isinstance<py::dict>(mapping))
		return py::reinterpret_borrow<py::dict>(mapping);

	return py::reinterpret_steal<py::dict>(
		py::module_::import("builtins").attr("dict")(mapping).release());
}

PyPlugin::PyPlugin()
{
	m_network = new QNetworkAccessManager(this);
	m_network->setCookieJar(new QNetworkCookieJar(m_network));

	connect(m_network, SIGNAL(finished(QNetworkReply*)), this, SLOT(fetchFinished(QNetworkReply*)));
}

PyPlugin::~PyPlugin()
{
	// Futures still pending here will never be resolved; the Task holding
	// them is going away with the plugin.
	m_fetches.clear();
}

void PyPlugin::logMessage(const QString& message)
{
	QString sender = m_strPluginName.isEmpty() ? QLatin1String("Python") : m_strPluginName;
	Logger::global()->enterLogMessage(sender, message);
}

void PyPlugin::abort()
{
	QList<QNetworkReply*> replies = m_fetches.keys();

	m_fetches.clear();

	foreach (QNetworkReply* reply, replies)
		reply->abort();
}

static QByteArray encodeFormData(py::handle data, bool* isForm)
{
	*isForm = false;

	if (py::isinstance<py::bytes>(data))
	{
		std::string s = data.cast<std::string>();
		return QByteArray(s.data(), s.size());
	}
	if (py::isinstance<py::str>(data))
	{
		*isForm = true;
		return QByteArray::fromStdString(data.cast<std::string>());
	}

	// A mapping: form-encode it, which is what a file host wants anyway.
	QUrlQuery query;
	py::dict dict = asDict(data);

	for (std::pair<py::handle, py::handle> item : dict)
	{
		QString key = QString::fromStdString(py::str(item.first).cast<std::string>());
		QString value = QString::fromStdString(py::str(item.second).cast<std::string>());

		query.addQueryItem(QUrl::toPercentEncoding(key), QUrl::toPercentEncoding(value));
	}

	*isForm = true;
	return query.toString(QUrl::FullyEncoded).toUtf8();
}

PyRef PyPlugin::fetch(const QString& url, const PyRef& data, const PyRef& headers, const QString& method)
{
	QNetworkRequest request = QNetworkRequest(QUrl(url));
	QByteArray body;
	bool hasBody = false, isForm = false;

	if (!data.isNull())
	{
		body = encodeFormData(pyFromRef(data), &isForm);
		hasBody = true;
	}

	request.setRawHeader("User-Agent", "FatRat/" VERSION);
	if (hasBody && isForm)
		request.setRawHeader("Content-Type", "application/x-www-form-urlencoded");

	if (!headers.isNull())
	{
		py::dict dict = asDict(pyFromRef(headers));

		for (std::pair<py::handle, py::handle> item : dict)
		{
			QString key = QString::fromStdString(py::str(item.first).cast<std::string>());
			QString value = QString::fromStdString(py::str(item.second).cast<std::string>());

			request.setRawHeader(key.toUtf8(), value.toUtf8());
		}
	}

	QNetworkReply* reply;
	QString verb = method.toUpper();

	if (!verb.isEmpty() && verb != QLatin1String("GET") && verb != QLatin1String("POST"))
		reply = m_network->sendCustomRequest(request, verb.toLatin1(), body);
	else if (hasBody || verb == QLatin1String("POST"))
		reply = m_network->post(request, body);
	else
		reply = m_network->get(request);

	logMessage(QString("fetch: %1").arg(url));
	qDebug() << "PyPlugin::fetch():" << url;

	// Cancelling the future (transfer stopped, plugin gave up on it) aborts
	// the request; the reply is QPointer'd because it may already be gone.
	QPointer<QNetworkReply> guard(reply);
	py::object future = pyMakeFuture([guard]()
	{
		if (guard)
			guard->abort();
	});

	m_fetches[reply] = pyToRef(future);

	return pyToRef(future);
}

/* Resuming a plugin coroutine can drop the last reference to the plugin --
 * the coroutine finishes, its frame goes away, and with it the Python object
 * that *is* this C++ object. Destroying a QObject from inside a signal that
 * its own child (the network manager) is still emitting crashes Qt, so hold
 * one extra reference and let the event loop drop it a turn later.
 */
static void keepAliveUntilEventLoopTurns(PyPlugin* plugin)
{
	PyRef self = pyToRef(py::cast(plugin));

	QMetaObject::invokeMethod(QCoreApplication::instance(), [self]() {}, Qt::QueuedConnection);
}

void PyPlugin::fetchFinished(QNetworkReply* reply)
{
	reply->deleteLater();

	QMap<QNetworkReply*, PyRef>::iterator it = m_fetches.find(reply);
	if (it == m_fetches.end())
		return; // aborted, or belongs to a plugin that has been stopped

	PyRef futureRef = it.value();
	m_fetches.erase(it);

	py::gil_scoped_acquire gil;

	keepAliveUntilEventLoopTurns(this);

	py::object future = pyFromRef(futureRef);

	try
	{
		if (reply->error() != QNetworkReply::NoError)
		{
			logMessage(QString("fetch failed: %1").arg(reply->errorString()));
			pyFailFuture(future, "NetworkError", reply->errorString());
			return;
		}

		QByteArray content = reply->readAll();
		py::dict headers;

		foreach (const QByteArray& name, reply->rawHeaderList())
		{
			headers[py::str(QString(name).toLower().toStdString())] =
				py::str(QString(reply->rawHeader(name)).trimmed().toStdString());
		}

		py::object response = pyModuleHttp().attr("Response")(
			py::arg("url") = reply->url().toString().toStdString(),
			py::arg("status") = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
			py::arg("content") = py::bytes(content.constData(), content.size()),
			py::arg("headers") = headers);

		// Resolving resumes the plugin coroutine, right here.
		pyResolveFuture(future, response);
	}
	catch (const py::error_already_set& e)
	{
		// The plugin raised out of the resumed coroutine and nothing above
		// us caught it; the Task reports it, this is the last resort.
		logMessage(pyErrorText(e));
	}
}

void registerPluginBindings(py::module_& m)
{
	py::class_<PyPlugin>(m, "PluginBase")
		.def(py::init<>())
		.def("fetch", [](PyPlugin& self, const std::string& url, py::object data,
				 py::object headers, py::object method)
		{
			PyRef dataRef = data.is_none() ? PyRef() : pyToRef(data);
			PyRef headerRef = headers.is_none() ? PyRef() : pyToRef(headers);
			QString verb = method.is_none() ? QString() : QString::fromStdString(py::str(method).cast<std::string>());

			return pyFromRef(self.fetch(QString::fromStdString(url), dataRef, headerRef, verb));
		}, py::arg("url"), py::arg("data") = py::none(), py::arg("headers") = py::none(),
		   py::arg("method") = py::none(),
		   "Fetch a page through FatRat. Await the result.")
		.def("log", [](PyPlugin& self, py::object message)
		{
			self.logMessage(QString::fromStdString(py::str(message).cast<std::string>()));
		}, py::arg("message"), "Write a line to this transfer's log.")
		.def_property_readonly("plugin_name", [](PyPlugin& self)
		{
			return self.pluginName().toStdString();
		});
}
