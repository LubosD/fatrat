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

#include "engines/PythonExtractor.h"

#include <QtDebug>

#include "Logger.h"
#include "TransferFactory.h"
#include "python/PyPlugin.h"
#include "util/LegacyRegex.h"

namespace py = pybind11;

QMap<QString, PythonExtractor::PyEngine>& PythonExtractor::engines()
{
	static QMap<QString, PyEngine> engines;
	return engines;
}

PythonExtractor::PythonExtractor(const char* identifier)
	: m_strIdentifier(identifier), m_plugin(0)
{
	if (!engines().contains(m_strIdentifier))
	{
		Logger::global()->enterLogMessage(QLatin1String("Python"),
			tr("No such plugin: %1").arg(m_strIdentifier));
		return;
	}

	const PyEngine& engine = engines()[m_strIdentifier];

	py::gil_scoped_acquire gil;

	try
	{
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

PythonExtractor::~PythonExtractor()
{
	py::gil_scoped_acquire gil;

	m_task.reset();
	m_plugin = 0;
	m_pluginObject.reset();
}

QString PythonExtractor::name() const
{
	return m_strUrl;
}

void PythonExtractor::init(QString source, QString target)
{
	m_strUrl = source;
	m_strTarget = target;
}

void PythonExtractor::load(const QDomNode& map)
{
	m_strUrl = getXMLProperty(map, "pyplugin_url");
	m_strTarget = getXMLProperty(map, "pyplugin_target");

	loadVars(map);

	Transfer::load(map);
}

void PythonExtractor::save(QDomDocument& doc, QDomNode& map) const
{
	Transfer::save(doc, map);

	saveVars(doc, map);

	setXMLProperty(doc, map, "pyplugin_url", m_strUrl);
	setXMLProperty(doc, map, "pyplugin_target", m_strTarget);
}

void PythonExtractor::failWith(const QString& message)
{
	setMessage(message);
	enterLogMessage(message);
	setState(Failed);
}

void PythonExtractor::changeActive(bool nowActive)
{
	py::gil_scoped_acquire gil;

	if (!nowActive)
	{
		if (!m_task.isNull())
		{
			py::object task = pyFromRef(m_task);
			m_task.reset();
			task.attr("cancel")();
		}

		if (m_plugin)
			m_plugin->abort();

		return;
	}

	if (!m_plugin)
	{
		failWith(tr("The plugin %1 could not be loaded").arg(m_strIdentifier));
		return;
	}

	try
	{
		// Unlike the Java extractors, the plugin fetches the page itself --
		// it already has fetch(), and some link lists need more than one
		// request to assemble.
		py::object coroutine = pyFromRef(m_pluginObject).attr("extract")(m_strUrl.toStdString());

		m_task = pyToRef(pySpawnTask(coroutine, [this](py::object result, py::object error)
		{
			m_task.reset();

			if (!error.is_none())
			{
				py::object cancelled = pyModuleAsync().attr("CancelledError");

				if (!py::isinstance(error, cancelled))
					failWith(QString::fromStdString(py::str(error).cast<std::string>()));
				return;
			}

			QStringList links;

			if (!result.is_none())
			{
				for (py::handle link : result)
					links << QString::fromStdString(py::str(link).cast<std::string>());
			}

			finishedExtraction(links);
		}));
	}
	catch (const py::error_already_set& e)
	{
		failWith(pyErrorText(e));
	}
}

void PythonExtractor::finishedExtraction(const QStringList& links)
{
	if (links.isEmpty())
	{
		failWith(tr("Empty or invalid link list"));
		return;
	}

	QString engineName = engines()[m_strIdentifier].targetClass;

	if (engineName.isEmpty())
	{
		// The plugin did not say which engine the links belong to, so ask
		// FatRat what it makes of the first one.
		BestEngine best = bestEngine(links[0], Download);

		if (!best.engine)
		{
			failWith(tr("Failed to detect URL class: %1").arg(links[0]));
			return;
		}

		engineName = best.engine->shortName;
	}

	QList<Transfer*> transfers;
	TransferFactory* factory = TransferFactory::instance();

	foreach (QString url, links)
	{
		Transfer* transfer = factory->createInstance(engineName);

		if (!transfer)
		{
			failWith(tr("No such download class: %1").arg(engineName));
			return;
		}

		transfer->init(url, m_strTarget);
		transfer->setState(Waiting);
		transfers << transfer;
	}

	enterLogMessage(tr("Extracted %1 links").arg(transfers.size()));

	QMetaObject::invokeMethod(this, "replaceItself", Qt::QueuedConnection,
				  Q_ARG(Transfer::TransferList, transfers));
}

int PythonExtractor::acceptable(QString uri, bool, const EngineEntry* e)
{
	return acceptableFor(QLatin1String(e->shortName), uri);
}

int PythonExtractor::acceptableFor(const QString& identifier, const QString& uri)
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
