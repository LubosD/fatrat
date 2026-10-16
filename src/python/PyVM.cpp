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

// pybind11 (really CPython's object.h) has members called `slots`, which Qt
// defines as a macro. Python headers therefore have to come before any Qt
// header -- in every file in this directory, not just this one.
#include <pybind11/embed.h>

#include "PyVM.h"

#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QtDebug>

#include "Logger.h"
#include "fatrat.h"

namespace py = pybind11;

PyVM* PyVM::m_instance = 0;

/* The native half of the plugin API.
 *
 * Everything Python can reach into FatRat with goes through this module, and
 * the shipped `fatrat` package is the only thing that imports it; plugins use
 * the Python wrapper. Keeping the native surface this narrow is what would let
 * the interpreter be moved out of process later (see the design document).
 *
 * Phase 0 exposes only what the startup check needs.
 */
PYBIND11_EMBEDDED_MODULE(_fatrat, m)
{
	m.doc() = "FatRat native plugin API (internal; use the `fatrat` package)";

	m.attr("__version__") = VERSION;
	m.attr("api_version") = "1";

	m.def("log", [](const std::string& message)
	{
		Logger::global()->enterLogMessage(QLatin1String("Python"), QString::fromStdString(message));
	}, py::arg("message"), "Write a line to FatRat's log.");
}

QString PyVM::runtimePath()
{
	// An uninstalled build tree has no <prefix>/share/fatrat yet; point this at
	// the source tree's data/python to run the shipped package from there.
	if (const char* env = getenv("FATRAT_PYTHON_PATH"))
	{
		if (*env)
			return QString::fromLocal8Bit(env);
	}
	return QLatin1String(DATA_LOCATION "/data/python");
}

QString PyVM::pluginsPath()
{
	return QDir::homePath() + QLatin1String(USER_PROFILE_PATH "/plugins");
}

QString PyVM::installedPluginsPath()
{
	return pluginsPath() + QLatin1String("/installed");
}

bool PyVM::available()
{
	return m_instance != 0 && m_instance->m_bReady;
}

PyVM::PyVM() : m_bReady(false), m_mainThreadState(0)
{
	if (!buildSearchPath())
		return;

	try
	{
		// false: leave the signal handlers alone, FatRat installs its own
		// (and a Python SIGINT handler would only run while Python code is
		// on the stack anyway).
		py::initialize_interpreter(false);
	}
	catch (const std::exception& e)
	{
		Logger::global()->enterLogMessage(QLatin1String("Python"),
			QObject::tr("Failed to initialize the Python interpreter: %1").arg(QString::fromUtf8(e.what())));
		return;
	}

	try
	{
		py::module_ sys = py::module_::import("sys");
		py::list path = sys.attr("path");

		for (int i = 0; i < m_searchPath.size(); i++)
			path.insert(i, m_searchPath[i].toStdString());

		// The shipped package is what plugins import; if it is missing or
		// broken, nothing else is going to work either.
		py::module_ fatrat = py::module_::import("fatrat");

		m_strVersion = QString::fromStdString(
			py::str(py::module_::import("platform").attr("python_version")()).cast<std::string>());

		qDebug() << "Python" << m_strVersion << "ready, fatrat package"
			 << QString::fromStdString(py::str(fatrat.attr("__version__")).cast<std::string>());
	}
	catch (const py::error_already_set& e)
	{
		Logger::global()->enterLogMessage(QLatin1String("Python"),
			QObject::tr("Failed to load the Python plugin API: %1").arg(QString::fromUtf8(e.what())));
		shutdown();
		return;
	}
	catch (const std::exception& e)
	{
		Logger::global()->enterLogMessage(QLatin1String("Python"),
			QObject::tr("Failed to load the Python plugin API: %1").arg(QString::fromUtf8(e.what())));
		shutdown();
		return;
	}

	m_bReady = true;

	Logger::global()->enterLogMessage(QLatin1String("Python"),
		QObject::tr("Python %1 extension support is ready").arg(m_strVersion));

	if (!m_instance)
		m_instance = this;

	// Hand the GIL back: FatRat spends almost all of its time in the Qt event
	// loop, and every call into Python takes the GIL again for its duration.
	m_mainThreadState = PyEval_SaveThread();
}

PyVM::~PyVM()
{
	shutdown();

	if (m_instance == this)
		m_instance = 0;
}

void PyVM::shutdown()
{
	if (m_mainThreadState)
	{
		PyEval_RestoreThread(static_cast<PyThreadState*>(m_mainThreadState));
		m_mainThreadState = 0;
	}

	if (Py_IsInitialized())
	{
		qDebug() << "Shutting down the Python interpreter...";
		py::finalize_interpreter();
	}

	m_bReady = false;
}

bool PyVM::buildSearchPath()
{
	QString runtime = runtimePath();

	if (!QFileInfo(runtime + QLatin1String("/fatrat/__init__.py")).isReadable())
	{
		Logger::global()->enterLogMessage(QLatin1String("Python"),
			QObject::tr("The Python plugin API was not found in \"%1\", extensions are disabled").arg(runtime));
		return false;
	}

	m_searchPath << runtime;

	// Installed plugins are imported as top-level modules/packages from here.
	// The directory legitimately does not exist until the first install.
	QString installed = installedPluginsPath();
	if (QDir(installed).exists())
		m_searchPath << installed;

	return true;
}
