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

#include "PluginRegistry.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QtDebug>

#include "Logger.h"
#include "PyVM.h"
#include "Settings.h"
#include "engines/PythonDownload.h"
#include "util/LegacyRegex.h"

namespace py = pybind11;

// Bump when the native API changes in a way plugins can see.
static const QLatin1String API_VERSION("1");

static QList<PluginRegistry::LoadedPlugin> s_loaded;

PluginRegistry::Manifest PluginRegistry::readManifest(const QString& directory)
{
	Manifest manifest;
	QFile file(QDir(directory).filePath(QLatin1String("plugin.json")));

	manifest.directory = directory;

	if (!file.open(QIODevice::ReadOnly))
	{
		manifest.error = QObject::tr("no plugin.json in %1").arg(directory);
		return manifest;
	}

	QJsonParseError parseError;
	QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);

	if (doc.isNull() || !doc.isObject())
	{
		manifest.error = QObject::tr("plugin.json is not valid JSON: %1").arg(parseError.errorString());
		return manifest;
	}

	QJsonObject root = doc.object();

	manifest.name = root.value(QLatin1String("name")).toString();
	manifest.version = root.value(QLatin1String("version")).toString();
	manifest.description = root.value(QLatin1String("description")).toString();
	manifest.entry = root.value(QLatin1String("entry")).toString();
	manifest.license = root.value(QLatin1String("license")).toString();
	manifest.homepage = root.value(QLatin1String("homepage")).toString();
	manifest.apiVersion = root.value(QLatin1String("api")).toString();
	manifest.minFatRat = root.value(QLatin1String("min_fatrat")).toString();

	QJsonArray permissions = root.value(QLatin1String("permissions")).toArray();
	for (QJsonArray::const_iterator it = permissions.begin(); it != permissions.end(); it++)
		manifest.permissions << (*it).toString();

	if (manifest.name.isEmpty())
	{
		manifest.error = QObject::tr("plugin.json has no name");
		return manifest;
	}

	if (manifest.entry.isEmpty())
		manifest.entry = manifest.name + QLatin1String(".py");

	// A plugin written against a different major API is not loaded at all:
	// half-working plugins are worse than absent ones.
	if (manifest.apiVersion.isEmpty())
		manifest.apiVersion = API_VERSION;
	if (manifest.apiVersion != API_VERSION)
	{
		manifest.error = QObject::tr("%1 needs plugin API %2, this FatRat has %3")
			.arg(manifest.name).arg(manifest.apiVersion).arg(API_VERSION);
		return manifest;
	}

	manifest.valid = true;
	return manifest;
}

QList<PluginRegistry::Manifest> PluginRegistry::installed()
{
	QList<Manifest> rv;
	QDir dir(PyVM::installedPluginsPath());

	if (!dir.exists())
		return rv;

	foreach (const QString& name, dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
		rv << readManifest(dir.filePath(name));

	return rv;
}

QList<PluginRegistry::LoadedPlugin> PluginRegistry::loaded()
{
	return s_loaded;
}

QStringList PluginRegistry::disabledPlugins()
{
	return getSettingsValue(QLatin1String("python/disabled_plugins")).toStringList();
}

bool PluginRegistry::isPluginEnabled(const QString& name)
{
	return !disabledPlugins().contains(name);
}

void PluginRegistry::setPluginEnabled(const QString& name, bool enabled)
{
	QStringList disabled = disabledPlugins();

	if (enabled)
		disabled.removeAll(name);
	else if (!disabled.contains(name))
		disabled << name;

	setSettingsValue(QLatin1String("python/disabled_plugins"), disabled);
}

/* Turn one decorated class into a FatRat engine.
 *
 * The identifier (module.ClassName) is what ends up in queues.xml, so it has
 * to stay stable across restarts; the strings behind EngineEntry's char
 * pointers live in the engine map, which is never cleared while FatRat runs.
 */
static QString registerDownloadEngine(py::handle descriptor)
{
	QString identifier = QString::fromStdString(py::str(descriptor.attr("identifier")).cast<std::string>());
	QString name = QString::fromStdString(py::str(descriptor.attr("name")).cast<std::string>());
	QString pattern = QString::fromStdString(py::str(descriptor.attr("url_pattern")).cast<std::string>());
	py::dict options = py::reinterpret_borrow<py::dict>(descriptor.attr("options"));

	PythonDownload::PyEngine engine;

	engine.name = (QObject::tr("EXT - ") + name).toStdString();
	engine.identifier = identifier.toStdString();
	engine.cls = pyToRef(descriptor.attr("cls"));
	engine.singleTransfer = options.contains("single_transfer") && options["single_transfer"].cast<bool>();
	engine.truncate = options.contains("truncate_incomplete") && options["truncate_incomplete"].cast<bool>();
	engine.ownAcceptable = options.contains("has_acceptable") && options["has_acceptable"].cast<bool>();

	if (!pattern.isEmpty())
		engine.regexp = legacyRegex(QRegularExpression::anchoredPattern(pattern));

	if (engine.ownAcceptable)
	{
		// acceptable() is asked about URLs long before any transfer exists,
		// so keep one instance around to ask.
		try
		{
			engine.probe = pyToRef(pyFromRef(engine.cls)());
		}
		catch (const py::error_already_set& e)
		{
			Logger::global()->enterLogMessage(QLatin1String("Python"),
				QObject::tr("%1 could not be instantiated: %2").arg(name).arg(pyErrorText(e)));
			engine.ownAcceptable = false;
		}
	}

	PythonDownload::engines()[identifier] = engine;

	const PythonDownload::PyEngine& stored = PythonDownload::engines()[identifier];
	EngineEntry entry;

	entry.shortName = stored.identifier.c_str();
	entry.longName = stored.name.c_str();
	entry.lpfnInit = 0;
	entry.lpfnExit = 0;
	entry.lpfnCreate2 = PythonDownload::createInstance;
	entry.lpfnAcceptable2 = PythonDownload::acceptable;
	entry.lpfnMultiOptions = 0;

	addTransferClass(entry, Transfer::Download);

	qDebug() << "Python plugin engine registered:" << identifier << name;

	return identifier;
}

PluginRegistry::LoadedPlugin PluginRegistry::loadPlugin(const Manifest& manifest)
{
	LoadedPlugin loaded;

	loaded.manifest = manifest;

	if (!manifest.valid)
	{
		loaded.error = manifest.error;
		return loaded;
	}

	py::gil_scoped_acquire gil;

	try
	{
		py::object descriptors = py::module_::import("fatrat._loader").attr("load")(
			manifest.name.toStdString(),
			manifest.directory.toStdString(),
			manifest.entry.toStdString());

		for (py::handle descriptor : descriptors)
		{
			QString kind = QString::fromStdString(py::str(descriptor.attr("kind")).cast<std::string>());

			if (kind == QLatin1String("download"))
				loaded.engines << registerDownloadEngine(descriptor);
			else
			{
				// Other kinds arrive in a later phase; say so rather than
				// letting the plugin look loaded when half of it is not.
				Logger::global()->enterLogMessage(QLatin1String("Python"),
					QObject::tr("%1: plugins of type \"%2\" are not supported yet")
						.arg(manifest.name).arg(kind));
			}
		}
	}
	catch (const py::error_already_set& e)
	{
		loaded.error = pyErrorText(e);
	}

	return loaded;
}

void PluginRegistry::globalInit()
{
	if (!PyVM::available())
		return;

	QList<Manifest> manifests = installed();

	foreach (const Manifest& manifest, manifests)
	{
		if (manifest.valid && !isPluginEnabled(manifest.name))
		{
			qDebug() << "Python plugin disabled by the user:" << manifest.name;
			continue;
		}

		LoadedPlugin loaded = loadPlugin(manifest);

		if (!loaded.error.isEmpty())
		{
			Logger::global()->enterLogMessage(QLatin1String("Python"),
				QObject::tr("Failed to load the plugin %1: %2")
					.arg(manifest.valid ? manifest.name : manifest.directory).arg(loaded.error));
		}
		else
		{
			Logger::global()->enterLogMessage(QLatin1String("Python"),
				QObject::tr("Loaded the plugin %1 %2 (%3 engines)")
					.arg(loaded.manifest.name).arg(loaded.manifest.version).arg(loaded.engines.size()));
		}

		s_loaded << loaded;
	}
}

void PluginRegistry::globalExit()
{
	py::gil_scoped_acquire gil;

	// Drops the references to the plugin classes and probe instances.
	PythonDownload::engines().clear();
	s_loaded.clear();
}
