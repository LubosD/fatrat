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

// Python headers before Qt; see src/python/PyRef.h.
#include "python/PyPython.h"

#include "engines/PythonDownload.h"

#include <QFile>
#include <QFileInfo>
#include <QtDebug>

#include "Logger.h"
#include "RuntimeException.h"
#include "fatrat.h"
#include "python/PyPlugin.h"
#ifdef WITH_GUI
#	include "engines/PythonDownloadOptsForm.h"
#endif

namespace py = pybind11;

QMap<QString, QSemaphore*> PythonDownload::m_slots;

QMap<QString, PythonDownload::PyEngine>& PythonDownload::engines()
{
	static QMap<QString, PyEngine> engines;
	return engines;
}

PythonDownload::PythonDownload(const char* identifier)
	: m_lease(QLatin1String(identifier)), m_strIdentifier(identifier), m_bHasLock(false), m_bTruncate(false),
	  m_bFinalChecked(false), m_bNameReported(false), m_plugin(0)
{
	if (!engines().contains(m_strIdentifier))
	{
		Logger::global()->enterLogMessage(QLatin1String("Python"),
			tr("No such plugin: %1").arg(m_strIdentifier));
		return;
	}

	const PyEngine& engine = engines()[m_strIdentifier];
	m_bTruncate = engine.truncate;

	py::gil_scoped_acquire gil;

	try
	{
		// The Python object and the C++ PyPlugin are one object; holding
		// the Python reference is what keeps this transfer's plugin alive.
		py::object instance = pyFromRef(engine.cls)();

		m_pluginObject = pyToRef(instance);
		m_plugin = instance.cast<PyPlugin*>();
		m_plugin->setPluginName(QString::fromStdString(engine.name));
		m_plugin->setTransfer(this);
	}
	catch (const py::error_already_set& e)
	{
		Logger::global()->enterLogMessage(QLatin1String("Python"),
			tr("Failed to create %1: %2").arg(m_strIdentifier).arg(pyErrorText(e)));
	}
}

PythonDownload::~PythonDownload()
{
	if (isActive())
		changeActive(false);

	py::gil_scoped_acquire gil;

	m_task.reset();
	m_plugin = 0;
	m_pluginObject.reset();
}

void PythonDownload::init(QString source, QString target)
{
	m_strOriginal = source;
	m_strTarget = target;
	deriveName();
}

QString PythonDownload::myClass() const
{
	return m_strIdentifier;
}

QString PythonDownload::name() const
{
	// Once the real download is running, curl knows the better name.
	if (!m_downloadUrl.isEmpty())
		return CurlDownload::name();
	return m_strName;
}

QString PythonDownload::remoteURI() const
{
	return m_strOriginal;
}

void PythonDownload::load(const QDomNode& map)
{
	m_strOriginal = getXMLProperty(map, "pyplugin_original");
	m_strTarget = getXMLProperty(map, "pyplugin_target");
	m_strName = getXMLProperty(map, "pyplugin_name");
	m_nTotal = getXMLProperty(map, "knowntotal").toLongLong();

	loadVars(map);

	if (m_strName.isEmpty())
		deriveName();

	Transfer::load(map);
}

void PythonDownload::save(QDomDocument& doc, QDomNode& map) const
{
	Transfer::save(doc, map);

	saveVars(doc, map);

	setXMLProperty(doc, map, "pyplugin_original", m_strOriginal);
	setXMLProperty(doc, map, "pyplugin_target", m_strTarget);
	setXMLProperty(doc, map, "pyplugin_name", m_strName);
	setXMLProperty(doc, map, "knowntotal", QString::number(m_nTotal));
}

void PythonDownload::deriveName()
{
	QString name = QFileInfo(m_strOriginal).fileName();

	name = QUrl::fromPercentEncoding(name.toUtf8());
	m_strName = (!name.isEmpty() && name != "/" && name != ".") ? name : m_strOriginal;
}

void PythonDownload::failWith(const QString& message)
{
	setMessage(message);
	enterLogMessage(message);
	Transfer::setState(Failed);
}

void PythonDownload::changeActive(bool bActive)
{
	if (!bActive)
	{
		if (m_bHasLock)
		{
			m_slots[m_strIdentifier]->release();
			m_bHasLock = false;
		}

		if (!m_downloadUrl.isEmpty())
			m_strName = CurlDownload::name();

		{
			py::gil_scoped_acquire gil;

			if (!m_task.isNull())
			{
				py::object task = pyFromRef(m_task);
				m_task.reset();
				task.attr("cancel")();
			}

			if (m_plugin && m_state == Failed)
			{
				try
				{
					pyFromRef(m_pluginObject).attr("on_failed")();
				}
				catch (const py::error_already_set& e)
				{
					enterLogMessage(pyErrorText(e));
				}
			}
		}

		if (m_plugin)
			m_plugin->abort();

		m_downloadUrl = QUrl();

		CurlDownload::changeActive(false);
		return;
	}

	if (!m_plugin)
	{
		failWith(tr("The plugin %1 could not be loaded").arg(m_strIdentifier));
		return;
	}

	const PyEngine& engine = engines()[m_strIdentifier];

	// Free accounts on file hosts usually allow one transfer, or a few.
	if (engine.maxTransfers > 0)
	{
		if (!m_slots[m_strIdentifier])
			m_slots[m_strIdentifier] = new QSemaphore(engine.maxTransfers);

		if (!m_bHasLock && !m_slots[m_strIdentifier]->tryAcquire())
		{
			failWith(tr("You cannot have multiple active transfers from this server."));
			return;
		}

		m_bHasLock = true;
	}

	if (engine.truncate)
	{
		// The resolved link is single-use: there is nothing to resume to.
		Segment s;

		s.offset = s.bytes = 0;
		s.urlIndex = 0;
		s.client = 0;

		m_segments.clear();
		m_segments << s;
	}

	m_bFinalChecked = false;

	py::gil_scoped_acquire gil;

	try
	{
		py::object coroutine = pyFromRef(m_pluginObject).attr("process_link")(m_strOriginal.toStdString());

		m_task = pyToRef(pySpawnTask(coroutine, [this](py::object, py::object error)
		{
			m_task.reset();

			if (error.is_none())
				return;

			// A cancelled coroutine is a stopped transfer, not a failure.
			py::object cancelled = pyModuleAsync().attr("CancelledError");
			if (py::isinstance(error, cancelled))
				return;

			failWith(QString::fromStdString(py::str(error).cast<std::string>()));
		}));
	}
	catch (const py::error_already_set& e)
	{
		failWith(pyErrorText(e));
	}
}

void PythonDownload::startDownload(const QString& url, const QString& referrer, const QString& userAgent,
				   const QString& postData, const QString& fileName)
{
	try
	{
		QString target = url;
		QString name = fileName;

		// A name from report_file_name() is the plugin's word on the file
		// name too; redirects to error pages must not get to rename it.
		if (name.isEmpty() && m_bNameReported)
			name = m_strName;

		if (!name.isEmpty())
		{
			QString safe = name;
			target += "#__filename=" + safe.replace('/', '-');
		}

		QList<QNetworkCookie> cookies = m_plugin->cookiesForUrl(QUrl(url));

		m_downloadUrl = target;
		m_listActiveSegments.clear();
		m_urls.clear();

		CurlDownload::init(target, m_strTarget);

		if (m_bTruncate)
		{
			Segment s;

			s.offset = s.bytes = 0;
			s.urlIndex = 0;
			s.client = 0;

			m_segments.clear();
			m_segments << s;
		}

		m_urls[0].cookies = cookies;
		m_urls[0].strReferrer = referrer;
		m_urls[0].strUserAgent = userAgent;
		m_urls[0].strPostData = postData.toUtf8();

		enterLogMessage(QLatin1String("start_download(): ") + url);

		// The plugin is done; the next transfer starts from a clean session.
		m_plugin->resetCookieJar();

		// The plugin's own status (a countdown, say) has served its purpose;
		// from here on the transfer list should show the download itself.
		CurlDownload::changeActive(true);
		m_strMessage.clear();
	}
	catch (const RuntimeException& e)
	{
		failWith(e.what());
	}
}

void PythonDownload::setObject(QString newdir)
{
	if (isActive())
		CurlDownload::setObject(newdir);
	m_strTarget = newdir;
}

qulonglong PythonDownload::done() const
{
	if (isActive() || (!m_bTruncate && !m_segments.isEmpty()))
		return CurlDownload::done();
	if (m_state == Completed)
		return m_nTotal;
	return 0;
}

void PythonDownload::setState(State newState)
{
	// File hosts answer "200 OK" with an HTML error page often enough that
	// the plugin gets the last word on whether this really completed.
	if (newState == Completed && !m_bFinalChecked && m_plugin)
	{
		m_bFinalChecked = true;

		py::gil_scoped_acquire gil;

		try
		{
			py::object coroutine = pyFromRef(m_pluginObject)
				.attr("final_check")(dataPath(true).toStdString());

			pySpawnTask(coroutine, [this](py::object, py::object error)
			{
				if (!error.is_none())
					failWith(QString::fromStdString(py::str(error).cast<std::string>()));

				if (m_state == Failed)
				{
					QFile::remove(dataPath(true));
					return;
				}

				Transfer::setState(Completed);
			});

			return;
		}
		catch (const py::error_already_set& e)
		{
			failWith(pyErrorText(e));
			QFile::remove(dataPath(true));
			return;
		}
	}

	Transfer::setState(newState);
}

int PythonDownload::acceptable(QString uri, bool, const EngineEntry* e)
{
	return acceptableFor(QLatin1String(e->shortName), uri);
}

int PythonDownload::acceptableFor(const QString& identifier, const QString& uri)
{
	if (!engines().contains(identifier))
		return 0;

	const PyEngine& engine = engines()[identifier];

	if (engine.ownAcceptable && !engine.probe.isNull())
	{
		py::gil_scoped_acquire gil;

		try
		{
			return pyFromRef(engine.probe).attr("acceptable")(uri.toStdString()).cast<int>();
		}
		catch (const py::error_already_set& error)
		{
			Logger::global()->enterLogMessage(QLatin1String("Python"),
				QObject::tr("%1 threw an exception in acceptable(): %2")
					.arg(QString::fromStdString(engine.name)).arg(pyErrorText(error)));
			return 0;
		}
	}

	if (!engine.regexp.pattern().isEmpty() && engine.regexp.match(uri).hasMatch())
		return 3;

	return 0;
}

#ifdef WITH_GUI
WidgetHostChild* PythonDownload::createOptionsWidget(QWidget* w)
{
	return new PythonDownloadOptsForm(w, this);
}
#endif
