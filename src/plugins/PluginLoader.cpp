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

#include "PluginLoader.h"
#include "PluginApi.h"
#include "TransferActions.h"

#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <dlfcn.h>
#include <exception>

// What only the desktop build exports. A plugin that links against one of them
// fails dlopen() in a headless build, which is not an error worth alarming anyone with.
static const char* const GuiSymbols[] = { "addMenuAction", "getMainWindow", "addStatusWidget", "removeStatusWidget", "WidgetHostDlg", "runProperties" };

static QString needsGui()
{
	return QObject::tr("needs the desktop interface");
}

PluginLoader::Result PluginLoader::load(const QString& path, bool haveGui)
{
	Result r;
	void* handle = dlopen(QFile::encodeName(path).constData(), RTLD_NOW);

	if (!handle)
	{
		QString error = QString::fromLocal8Bit(dlerror());
		// Look only at the symbol name: the plugin's path could contain one of ours.
		static const QLatin1String marker("undefined symbol: ");
		int at = error.indexOf(marker);
		QString symbolText = at >= 0 ? error.mid(at + marker.size()) : QString();

		for (const char* symbol : GuiSymbols)
		{
			if (symbolText.contains(QLatin1String(symbol)))
			{
				r.outcome = SkippedNeedsGui;
				r.message = needsGui();
				return r;
			}
		}
		r.message = error;
		return r;
	}

	PluginInfo (*info)() = (PluginInfo (*)()) dlsym(handle, "getInfo");
	int (*apiVersion)() = (int (*)()) dlsym(handle, "getApiVersion");
	bool initStarted = false;

	// Everything below calls into the plugin, which may throw. Until initPlugin()
	// starts nothing of the plugin's is registered, so it can be unloaded again.
	try
	{
		if (!apiVersion)
		{
			// From before the plugin API. Its static constructors have run, so it
			// stays mapped whether it is used or not.
			if (!haveGui)
			{
				r.outcome = SkippedNeedsGui;
				r.message = needsGui();
				return r;
			}
			r.outcome = LoadedLegacy;
			if (info)
				r.info = info();
			return r;
		}

		bool (*requiresGui)() = (bool (*)()) dlsym(handle, "requiresGui");
		void (*init)() = (void (*)()) dlsym(handle, "initPlugin");
		int version = apiVersion();

		if (version != FATRAT_PLUGIN_API_VERSION)
			r.message = QObject::tr("built for plugin API %1, this FatRat has %2").arg(version).arg(FATRAT_PLUGIN_API_VERSION);
		else if (!requiresGui)
			r.message = QObject::tr("exports no requiresGui()");
		else if (!init)
			r.message = QObject::tr("exports no initPlugin()");

		if (!r.message.isEmpty())
		{
			dlclose(handle);
			r.outcome = Incompatible;
			return r;
		}

		if (requiresGui() && !haveGui)
		{
			dlclose(handle);
			r.outcome = SkippedNeedsGui;
			r.message = needsGui();
			return r;
		}

		// Before initPlugin(), so that a getInfo() that throws leaves nothing registered.
		std::optional<PluginInfo> pluginInfo;
		if (info)
			pluginInfo = info();

		QString name = QFileInfo(path).fileName();
		initStarted = true;
		TransferActions::setLoadingPlugin(name);
		try
		{
			init();
		}
		catch (...)
		{
			// Whatever it registered before throwing would point at code that is
			// about to stay mapped only by our choice; drop it.
			TransferActions::setLoadingPlugin(QString());
			TransferActions::removePlugin(name);
			throw;
		}
		TransferActions::setLoadingPlugin(QString());

		r.outcome = Loaded;
		r.info = pluginInfo;
	}
	catch (const std::exception& e)
	{
		r.outcome = Failed;
		r.message = QString::fromUtf8(e.what());
		r.info.reset();
		// Left mapped once initPlugin() has started: it may have started threads or the like.
		if (!initStarted && apiVersion)
			dlclose(handle);
	}
	catch (...)
	{
		r.outcome = Failed;
		r.message = QObject::tr("unknown error");
		r.info.reset();
		if (!initStarted && apiVersion)
			dlclose(handle);
	}

	return r;
}
