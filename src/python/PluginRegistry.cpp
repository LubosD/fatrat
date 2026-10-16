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
#include <QObject>
#include <QtDebug>

#include "Logger.h"
#include "PluginServices.h"
#include "PyVM.h"
#include "Settings.h"
#ifdef WITH_GUI
#	include "engines/SettingsPythonPluginForm.h"
#endif
#include "engines/PythonDownload.h"
#include "engines/PythonExtractor.h"
#include "engines/PythonUpload.h"
#include "util/LegacyRegex.h"

namespace py = pybind11;

// Bump when the native API changes in a way plugins can see.
static const QLatin1String API_VERSION("1");

static QList<PluginRegistry::LoadedPlugin> s_loaded;

QString PluginRegistry::apiVersion()
{
	return API_VERSION;
}

QList<PluginRegistry::Manifest> PluginRegistry::installed()
{
	return PluginManifest::readAll(PyVM::installedPluginsPath(), API_VERSION);
}

QList<PluginRegistry::LoadedPlugin> PluginRegistry::loaded()
{
	return s_loaded;
}

/* One ordinary settings key per plugin, rather than a list of disabled ones:
 * that way every interface -- the desktop dialog, the web UI, the TUI, even
 * fatrat-conf -- can read and write it like any other setting.
 */
QString PluginRegistry::enabledKey(const QString& name)
{
	return QLatin1String("python/plugins/") + name + QLatin1String("/enabled");
}

bool PluginRegistry::isPluginEnabled(const QString& name)
{
	return getSettingsValue(enabledKey(name), true).toBool();
}

void PluginRegistry::setPluginEnabled(const QString& name, bool enabled)
{
	setSettingsValue(enabledKey(name), enabled);
}

/* Turn one decorated class into a FatRat engine.
 *
 * The identifier (module.ClassName) is what ends up in queues.xml, so it has
 * to stay stable across restarts; the strings behind EngineEntry's char
 * pointers live in the engine map, which is never cleared while FatRat runs.
 */
namespace {

// The parts of registration that do not depend on the plugin kind.
struct CommonEngine
{
	QString identifier, name, pattern;
	py::dict options;
	PyRef cls, probe;
	QRegularExpression regexp;
	bool ownAcceptable;
};

CommonEngine readDescriptor(py::handle descriptor)
{
	CommonEngine common;

	common.identifier = QString::fromStdString(py::str(descriptor.attr("identifier")).cast<std::string>());
	common.name = QString::fromStdString(py::str(descriptor.attr("name")).cast<std::string>());
	common.pattern = QString::fromStdString(py::str(descriptor.attr("url_pattern")).cast<std::string>());
	common.options = py::reinterpret_borrow<py::dict>(descriptor.attr("options"));
	common.cls = pyToRef(descriptor.attr("cls"));
	common.ownAcceptable = common.options.contains("has_acceptable")
		&& common.options["has_acceptable"].cast<bool>();

	if (!common.pattern.isEmpty())
		common.regexp = legacyRegex(QRegularExpression::anchoredPattern(common.pattern));

	if (common.ownAcceptable)
	{
		// acceptable() is asked about URLs long before any transfer exists,
		// so keep one instance around to ask.
		try
		{
			common.probe = pyToRef(pyFromRef(common.cls)());
		}
		catch (const py::error_already_set& e)
		{
			Logger::global()->enterLogMessage(QLatin1String("Python"),
				QObject::tr("%1 could not be instantiated: %2").arg(common.name).arg(pyErrorText(e)));
			common.ownAcceptable = false;
		}
	}

	return common;
}

QString optionString(const py::dict& options, const char* key)
{
	if (!options.contains(key))
		return QString();

	return QString::fromStdString(py::str(options[key]).cast<std::string>());
}

bool optionBool(const py::dict& options, const char* key)
{
	return options.contains(key) && options[key].cast<bool>();
}

} // namespace

static QString registerDownloadEngine(py::handle descriptor)
{
	CommonEngine common = readDescriptor(descriptor);
	PythonDownload::PyEngine engine;

	engine.name = (QObject::tr("EXT - ") + common.name).toStdString();
	engine.identifier = common.identifier.toStdString();
	engine.cls = common.cls;
	engine.probe = common.probe;
	engine.regexp = common.regexp;
	engine.ownAcceptable = common.ownAcceptable;
	engine.singleTransfer = optionBool(common.options, "single_transfer");
	engine.truncate = optionBool(common.options, "truncate_incomplete");

	PythonDownload::engines()[common.identifier] = engine;

	const PythonDownload::PyEngine& stored = PythonDownload::engines()[common.identifier];
	EngineEntry entry;

	entry.shortName = stored.identifier.c_str();
	entry.longName = stored.name.c_str();
	entry.lpfnInit = 0;
	entry.lpfnExit = 0;
	entry.lpfnCreate2 = PythonDownload::createInstance;
	entry.lpfnAcceptable2 = PythonDownload::acceptable;
	entry.lpfnMultiOptions = 0;

	addTransferClass(entry, Transfer::Download);

	qDebug() << "Python plugin engine registered:" << common.identifier << common.name;

	return common.identifier;
}

// Search and account-status plugins register no engine: they are asked a
// question when the user opens the search tool or the account list.
static QString registerServicePlugin(py::handle descriptor, QList<PluginServices::Entry>& into)
{
	CommonEngine common = readDescriptor(descriptor);
	PluginServices::Entry entry;

	entry.identifier = common.identifier;
	entry.name = common.name;
	entry.cls = common.cls;

	into << entry;

	qDebug() << "Python service plugin registered:" << common.identifier << common.name;

	return common.identifier;
}

static QString registerUploadEngine(py::handle descriptor)
{
	CommonEngine common = readDescriptor(descriptor);
	PythonUpload::PyEngine engine;

	engine.name = (QObject::tr("EXT - ") + common.name).toStdString();
	engine.identifier = common.identifier.toStdString();
	engine.cls = common.cls;

	PythonUpload::engines()[common.identifier] = engine;

	const PythonUpload::PyEngine& stored = PythonUpload::engines()[common.identifier];
	EngineEntry entry;

	entry.shortName = stored.identifier.c_str();
	entry.longName = stored.name.c_str();
	entry.lpfnInit = 0;
	entry.lpfnExit = 0;
	entry.lpfnCreate2 = PythonUpload::createInstance;
	entry.lpfnAcceptable2 = PythonUpload::acceptable;
	entry.lpfnMultiOptions = 0;

	addTransferClass(entry, Transfer::Upload);

	qDebug() << "Python upload engine registered:" << common.identifier << common.name;

	return common.identifier;
}

static QString registerExtractorEngine(py::handle descriptor)
{
	CommonEngine common = readDescriptor(descriptor);
	PythonExtractor::PyEngine engine;

	engine.name = (QObject::tr("EXT - ") + common.name).toStdString();
	engine.identifier = common.identifier.toStdString();
	engine.cls = common.cls;
	engine.probe = common.probe;
	engine.regexp = common.regexp;
	engine.ownAcceptable = common.ownAcceptable;
	engine.targetClass = optionString(common.options, "transfer_class");

	PythonExtractor::engines()[common.identifier] = engine;

	const PythonExtractor::PyEngine& stored = PythonExtractor::engines()[common.identifier];
	EngineEntry entry;

	entry.shortName = stored.identifier.c_str();
	entry.longName = stored.name.c_str();
	entry.lpfnInit = 0;
	entry.lpfnExit = 0;
	entry.lpfnCreate2 = PythonExtractor::createInstance;
	entry.lpfnAcceptable2 = PythonExtractor::acceptable;
	entry.lpfnMultiOptions = 0;

	addTransferClass(entry, Transfer::Download);

	qDebug() << "Python extractor engine registered:" << common.identifier << common.name;

	return common.identifier;
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
			else if (kind == QLatin1String("extractor"))
				loaded.engines << registerExtractorEngine(descriptor);
			else if (kind == QLatin1String("upload"))
				loaded.engines << registerUploadEngine(descriptor);
			else if (kind == QLatin1String("search"))
				loaded.engines << registerServicePlugin(descriptor, PluginServices::searchPlugins());
			else if (kind == QLatin1String("account_status"))
				loaded.engines << registerServicePlugin(descriptor, PluginServices::accountStatusPlugins());
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

	// Settings -> Extensions exists whether or not anything is installed:
	// it is where a user goes to install the first plugin.
	SettingsItem page;

	page.title = QObject::tr("Extensions");
	page.icon = DelayedIcon(QLatin1String(":/menu/newtool.png"));
#ifdef WITH_GUI
	page.lpfnCreate = SettingsPythonPluginForm::create;
#endif
#ifdef WITH_WEBINTERFACE
	// The same page in the web interface, rendered from the same manifests.
	page.webSettingsScript = "/scripts/settings/extensions.js";
	page.webSettingsIconURL = "/img/settings/extensions.png";
#endif

	addSettingsPage(page);
}

void PluginRegistry::notifyConfigSaved()
{
	if (!PyVM::available())
		return;

	py::gil_scoped_acquire gil;

	try
	{
		py::module_::import("fatrat.registry").attr("notify_config_saved")();
	}
	catch (const py::error_already_set& e)
	{
		Logger::global()->enterLogMessage(QLatin1String("Python"),
			QObject::tr("A plugin failed in on_config_saved(): %1").arg(pyErrorText(e)));
	}
}

void PluginRegistry::globalExit()
{
	py::gil_scoped_acquire gil;

	// Drops the references to the plugin classes and probe instances.
	PythonDownload::engines().clear();
	PythonExtractor::engines().clear();
	PythonUpload::engines().clear();
	PluginServices::searchPlugins().clear();
	PluginServices::accountStatusPlugins().clear();
	s_loaded.clear();
}
