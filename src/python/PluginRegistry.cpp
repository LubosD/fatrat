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

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QObject>
#include <QPair>
#include <QRecursiveMutex>
#include <QThread>
#include <QtDebug>
#include <functional>
#include <set>

#include "Logger.h"
#include "PluginLease.h"
#include "PluginRepository.h"
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

// Guards s_loaded and s_side: the REST threads read them while the main
// thread loads and unloads. Never held across a call into Python.
static QRecursiveMutex s_mutex;
static QList<PluginRegistry::LoadedPlugin> s_loaded;

/* What a plugin put into FatRat's tables, so it can be taken out again (when
 * the plugin is unloaded or starts draining) and put back (when it is enabled
 * again while it still drains). Kept here, not in LoadedPlugin: the header
 * is used where Transfer.h is not.
 */
namespace {
struct Registered
{
	Registered() : reloadAfter(false), uninstallAfter(false) {}

	QList<QPair<int, EngineEntry> > engines; // Transfer::Mode and its entry
	QList<PluginServices::Entry> search, accountStatus;
	bool reloadAfter, uninstallAfter;
};
}

static QHash<QString, Registered> s_side;

// While loadPlugin() runs: where its register*() calls report to.
static Registered* s_collecting = 0;

/* EngineEntry keeps borrowed char pointers, and engine slots can outlive the
 * plugin's own record, so the strings live for the whole process.
 */
static const char* intern(const std::string& value)
{
	static std::set<std::string> strings;
	return strings.insert(value).first->c_str();
}

// Run on the main thread, waiting for it: the engine tables are not locked.
static void runOnMain(const std::function<void()>& fn)
{
	QCoreApplication* app = QCoreApplication::instance();

	if (!app || QThread::currentThread() == app->thread())
	{
		fn();
		return;
	}

	QMetaObject::invokeMethod(app, [&] { fn(); }, Qt::BlockingQueuedConnection);
}

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
	QMutexLocker locker(&s_mutex);
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

	// Takes effect now, not at the next start.
	if (PyVM::available())
		sync(name);
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

int optionInt(const py::dict& options, const char* key)
{
	return options.contains(key) ? options[key].cast<int>() : 0;
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
	engine.maxTransfers = qMax(0, optionInt(common.options, "single_transfer"));
	engine.truncate = optionBool(common.options, "truncate_incomplete");

	PythonDownload::engines()[common.identifier] = engine;

	EngineEntry entry;

	entry.shortName = intern(engine.identifier);
	entry.longName = intern(engine.name);
	entry.lpfnInit = 0;
	entry.lpfnExit = 0;
	entry.lpfnCreate2 = PythonDownload::createInstance;
	entry.lpfnAcceptable2 = PythonDownload::acceptable;
	entry.lpfnMultiOptions = 0;

	entry.removed = false;
	addTransferClass(entry, Transfer::Download);

	if (s_collecting)
		s_collecting->engines << qMakePair(int(Transfer::Download), entry);

	qDebug() << "Python plugin engine registered:" << common.identifier << common.name;

	return common.identifier;
}

// Search and account-status plugins register no engine: they are asked a
// question when the user opens the search tool or the account list.
static QString registerServicePlugin(py::handle descriptor, QList<PluginServices::Entry>& into,
				     QList<PluginServices::Entry>* record)
{
	CommonEngine common = readDescriptor(descriptor);
	PluginServices::Entry entry;

	entry.identifier = common.identifier;
	entry.name = common.name;
	entry.cls = common.cls;

	into << entry;

	if (record)
		*record << entry;

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

	EngineEntry entry;

	entry.shortName = intern(engine.identifier);
	entry.longName = intern(engine.name);
	entry.lpfnInit = 0;
	entry.lpfnExit = 0;
	entry.lpfnCreate2 = PythonUpload::createInstance;
	entry.lpfnAcceptable2 = PythonUpload::acceptable;
	entry.lpfnMultiOptions = 0;

	entry.removed = false;
	addTransferClass(entry, Transfer::Upload);

	if (s_collecting)
		s_collecting->engines << qMakePair(int(Transfer::Upload), entry);

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

	EngineEntry entry;

	entry.shortName = intern(engine.identifier);
	entry.longName = intern(engine.name);
	entry.lpfnInit = 0;
	entry.lpfnExit = 0;
	entry.lpfnCreate2 = PythonExtractor::createInstance;
	entry.lpfnAcceptable2 = PythonExtractor::acceptable;
	entry.lpfnMultiOptions = 0;

	entry.removed = false;
	addTransferClass(entry, Transfer::Download);

	if (s_collecting)
		s_collecting->engines << qMakePair(int(Transfer::Download), entry);

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

	Registered registered;

	s_collecting = &registered;

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
				loaded.engines << registerServicePlugin(descriptor, PluginServices::searchPlugins(),
					s_collecting ? &s_collecting->search : 0);
			else if (kind == QLatin1String("account_status"))
				loaded.engines << registerServicePlugin(descriptor, PluginServices::accountStatusPlugins(),
					s_collecting ? &s_collecting->accountStatus : 0);
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

	s_collecting = 0;

	{
		QMutexLocker locker(&s_mutex);
		s_side[manifest.name] = registered;
	}

	return loaded;
}

void PluginRegistry::reportLegacyJavaPlugins()
{
	QDir dir(QDir::homePath() + QLatin1String(USER_PROFILE_PATH "/data/java"));
	QStringList jars = dir.entryList(QStringList() << QLatin1String("*.jar"), QDir::Files);

	if (jars.isEmpty())
		return;

	Logger::global()->enterLogMessage(QLatin1String("Python"),
		QObject::tr("The Java extensions are gone, and the %1 jar files in %2 are no longer used. "
			    "Extensions are now written in Python and installed from plugin repositories you add "
			    "yourself (Settings -> Extensions).")
			.arg(jars.size()).arg(dir.path()));
}

static void loadAndRecord(const PluginRegistry::Manifest& manifest)
{
	PluginRegistry::LoadedPlugin loaded = PluginRegistry::loadPlugin(manifest);

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

	QMutexLocker locker(&s_mutex);
	s_loaded << loaded;
}

static int indexOfLoaded(const QString& name)
{
	for (int i = 0; i < s_loaded.size(); i++)
	{
		if (s_loaded[i].manifest.name == name)
			return i;
	}

	return -1;
}

static int transfersOf(const PluginRegistry::LoadedPlugin& plugin)
{
	int n = 0;

	foreach (const QString& identifier, plugin.engines)
		n += PluginLease::count(identifier);

	return n;
}

// Take a plugin's engines out of everything that offers them for new work.
// The plugin stays imported, and its transfers keep running.
static void withdraw(const QString& name)
{
	py::gil_scoped_acquire gil;
	QMutexLocker locker(&s_mutex);
	int index = indexOfLoaded(name);

	if (index < 0 || s_loaded[index].draining)
		return;

	const Registered& side = s_side[name];

	for (int i = 0; i < side.engines.size(); i++)
	{
		removeTransferClass(side.engines[i].second.shortName,
			static_cast<Transfer::Mode>(side.engines[i].first));
	}

	for (int kind = 0; kind < 2; kind++)
	{
		const QList<PluginServices::Entry>& mine = kind ? side.accountStatus : side.search;
		QList<PluginServices::Entry>& all = kind ? PluginServices::accountStatusPlugins()
							 : PluginServices::searchPlugins();

		for (int i = 0; i < mine.size(); i++)
		{
			for (int j = all.size() - 1; j >= 0; j--)
			{
				if (all[j].identifier == mine[i].identifier)
					all.removeAt(j);
			}
		}
	}

	s_loaded[index].draining = true;
}

// The reverse of withdraw(), for a plugin enabled again before it unloaded.
static void reactivate(const QString& name)
{
	py::gil_scoped_acquire gil;
	QMutexLocker locker(&s_mutex);
	int index = indexOfLoaded(name);

	if (index < 0 || !s_loaded[index].draining)
		return;

	const Registered& side = s_side[name];

	for (int i = 0; i < side.engines.size(); i++)
		addTransferClass(side.engines[i].second, static_cast<Transfer::Mode>(side.engines[i].first));

	PluginServices::searchPlugins() << side.search;
	PluginServices::accountStatusPlugins() << side.accountStatus;

	s_loaded[index].draining = false;
}

// Forget a withdrawn plugin whose transfers are all gone, then do whatever
// was waiting for that.
static void unloadNow(const QString& name)
{
	QStringList identifiers;
	bool reloadAfter, uninstallAfter;

	{
		QMutexLocker locker(&s_mutex);
		int index = indexOfLoaded(name);

		if (index < 0)
			return;

		identifiers = s_loaded[index].engines;
		reloadAfter = s_side[name].reloadAfter;
		uninstallAfter = s_side[name].uninstallAfter;
		s_loaded.removeAt(index);
		s_side.remove(name);
	}

	{
		py::gil_scoped_acquire gil;

		foreach (const QString& identifier, identifiers)
		{
			PythonDownload::engines().remove(identifier);
			PythonExtractor::engines().remove(identifier);
			PythonUpload::engines().remove(identifier);
		}

		try
		{
			py::module_::import("fatrat._loader").attr("unload")(name.toStdString());
		}
		catch (const py::error_already_set& e)
		{
			Logger::global()->enterLogMessage(QLatin1String("Python"),
				QObject::tr("Unloading the plugin %1 failed: %2").arg(name).arg(pyErrorText(e)));
		}
	}

	Logger::global()->enterLogMessage(QLatin1String("Python"),
		QObject::tr("Unloaded the plugin %1").arg(name));

	if (uninstallAfter)
	{
		QString error;

		if (!PluginRepository::uninstall(name, &error))
		{
			Logger::global()->enterLogMessage(QLatin1String("Python"),
				QObject::tr("Failed to remove the plugin %1: %2").arg(name).arg(error));
		}
	}
	else if (reloadAfter)
		PluginRegistry::sync(name);
}

void PluginRegistry::sync(const QString& name)
{
	if (!PyVM::available())
		return;

	runOnMain([name] {
		Manifest manifest;
		bool found = false;

		foreach (const Manifest& candidate, installed())
		{
			if (candidate.valid && candidate.name == name)
			{
				manifest = candidate;
				found = true;
				break;
			}
		}

		bool wanted = found && isPluginEnabled(name);
		bool present, draining, deferred;

		{
			QMutexLocker locker(&s_mutex);
			int index = indexOfLoaded(name);

			present = (index >= 0);
			draining = present && s_loaded[index].draining;
			deferred = present && (s_side[name].reloadAfter || s_side[name].uninstallAfter);
		}

		if (wanted)
		{
			if (!present)
				loadAndRecord(manifest);
			else if (draining && !deferred)
				reactivate(name); // changed its mind in time
		}
		else if (present)
		{
			withdraw(name);
			finishDrains();
		}
	});
}

void PluginRegistry::syncAll()
{
	foreach (const Manifest& manifest, installed())
	{
		if (manifest.valid)
			sync(manifest.name);
	}
}

void PluginRegistry::reload(const QString& name)
{
	if (!PyVM::available())
		return;

	runOnMain([name] {
		bool present;

		{
			QMutexLocker locker(&s_mutex);
			present = indexOfLoaded(name) >= 0;

			if (present)
				s_side[name].reloadAfter = true;
		}

		if (!present)
		{
			sync(name);
			return;
		}

		withdraw(name);
		finishDrains();
	});
}

bool PluginRegistry::uninstall(const QString& name, QString* error)
{
	bool deferred = false;

	if (PyVM::available())
	{
		runOnMain([name, &deferred] {
			{
				QMutexLocker locker(&s_mutex);
				int index = indexOfLoaded(name);

				if (index < 0)
					return;

				s_side[name].uninstallAfter = true;
				deferred = true;
			}

			withdraw(name);

			// Unloads at once if nothing uses it, and removes the files.
			finishDrains();

			QMutexLocker locker(&s_mutex);
			deferred = (indexOfLoaded(name) >= 0);
		});

		if (deferred)
			return true;
	}

	QString ignored;

	// Not loaded (disabled, failed, no interpreter), or finishDrains() has
	// unloaded it and removed the files already, which this then finds
	// nothing left to do about.
	return PluginRepository::uninstall(name, error ? error : &ignored);
}

PluginRegistry::Status PluginRegistry::status(const QString& name)
{
	QMutexLocker locker(&s_mutex);
	Status rv;
	int index = indexOfLoaded(name);

	if (index < 0)
		return rv;

	const LoadedPlugin& plugin = s_loaded[index];

	rv.loaded = true;
	rv.draining = plugin.draining;
	rv.error = plugin.error;
	rv.activeTransfers = transfersOf(plugin);
	rv.pendingReload = s_side[name].reloadAfter;
	rv.pendingUninstall = s_side[name].uninstallAfter;

	return rv;
}

void PluginRegistry::finishDrains()
{
	if (!PyVM::available())
		return;

	runOnMain([] {
		QStringList idle;

		{
			QMutexLocker locker(&s_mutex);

			foreach (const LoadedPlugin& plugin, s_loaded)
			{
				if (plugin.draining && transfersOf(plugin) == 0)
					idle << plugin.manifest.name;
			}
		}

		foreach (const QString& name, idle)
			unloadNow(name);
	});
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

		loadAndRecord(manifest);
	}

	// Settings -> Extensions exists whether or not anything is installed:
	// it is where a user goes to install the first plugin.
	SettingsItem page;

	page.title = QObject::tr("Extensions");
	page.icon = DelayedIcon(QLatin1String(":/menu/newtool.png"));
	// Settings saved from anywhere may have flipped a plugin's "enabled" key.
	page.pfnApply = [] { PluginRegistry::syncAll(); };
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
	s_side.clear();
	s_loaded.clear();
}
