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

#include <QCoreApplication>
#include <QRunnable>
#include <QSettings>
#include <QThreadPool>
#include <QtDebug>

#include "Logger.h"
#include "Settings.h"
#include "fatrat.h"

namespace py = pybind11;

/* fatrat.run_in_thread(): run a blocking or CPU-bound callable off the main
 * thread and resolve a Future with its result.
 *
 * This is the only place plugin code runs anywhere but the main thread, and
 * the callable is not allowed to touch FatRat's API from there -- it computes
 * and returns, nothing else. (The old JBackgroundWorker had the same rule.)
 */
class PyWorkerTask : public QRunnable
{
public:
	PyWorkerTask(const PyRef& callable, const PyRef& future)
		: m_callable(callable), m_future(future)
	{
		setAutoDelete(true);
	}

	virtual void run()
	{
		PyRef result, error;

		{
			py::gil_scoped_acquire gil;
			try
			{
				result = pyToRef(pyFromRef(m_callable)());
			}
			catch (const py::error_already_set& e)
			{
				error = pyToRef(e.value());
			}
		}

		PyRef future = m_future;

		// Back to the main thread: a Future must only ever be resolved
		// there, because resolving it resumes the plugin's coroutine.
		QMetaObject::invokeMethod(QCoreApplication::instance(), [future, result, error]()
		{
			py::gil_scoped_acquire gil;
			py::object fut = pyFromRef(future);

			if (!error.isNull())
				fut.attr("set_exception")(pyFromRef(error));
			else
				fut.attr("set_result")(pyFromRef(result));
		}, Qt::QueuedConnection);
	}
private:
	PyRef m_callable, m_future;
};

void registerCoreBindings(py::module_& m)
{
	m.attr("__version__") = VERSION;
	m.attr("api_version") = "1";

	m.def("log", [](const std::string& message)
	{
		Logger::global()->enterLogMessage(QLatin1String("Python"), QString::fromStdString(message));
	}, py::arg("message"), "Write a line to FatRat's log.");

	m.def("settings_get", [](const std::string& key, py::object def) -> py::object
	{
		// A sentinel keeps "unset" distinguishable from "set to an empty
		// string", which QSettings alone cannot tell us.
		static const QString sentinel = QLatin1String("\x01----fatrat-unset");
		QVariant value = getSettingsValue(QString::fromStdString(key), sentinel);

		if (value.userType() == QMetaType::QString && value.toString() == sentinel)
			return def;

		return pyFromVariant(value);
	}, py::arg("key"), py::arg("default") = py::none());

	m.def("settings_set", [](const std::string& key, py::handle value)
	{
		setSettingsValue(QString::fromStdString(key), pyToVariant(value));
	}, py::arg("key"), py::arg("value"));

	m.def("settings_remove", [](const std::string& key)
	{
		if (g_settings)
			g_settings->remove(QString::fromStdString(key));
	}, py::arg("key"));

	m.def("settings_get_array", [](const std::string& key)
	{
		py::list rv;
		QList<QMap<QString, QVariant> > items = getSettingsArray(QString::fromStdString(key));

		for (int i = 0; i < items.size(); i++)
			rv.append(pyFromVariant(items[i].value(QLatin1String("item"))));

		return rv;
	}, py::arg("key"));

	m.def("run_in_thread", [](py::object callable)
	{
		py::object future = pyMakeFuture();

		QThreadPool::globalInstance()->start(new PyWorkerTask(pyToRef(callable), pyToRef(future)));

		return future;
	}, py::arg("callable"));
}
