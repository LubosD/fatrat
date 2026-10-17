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
static const char* const GuiSymbols[] = { "addMenuAction", "getMainWindow", "addStatusWidget", "removeStatusWidget" };

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

		for (const char* symbol : GuiSymbols)
		{
			if (error.contains(QLatin1String(symbol)))
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
	else if (!init)
		r.message = QObject::tr("exports no initPlugin()");

	if (!r.message.isEmpty())
	{
		dlclose(handle);
		r.outcome = Incompatible;
		return r;
	}

	if (requiresGui && requiresGui() && !haveGui)
	{
		dlclose(handle);
		r.outcome = SkippedNeedsGui;
		r.message = needsGui();
		return r;
	}

	TransferActions::setLoadingPlugin(QFileInfo(path).fileName());
	try
	{
		init();
		r.outcome = Loaded;
		if (info)
			r.info = info();
	}
	catch (const std::exception& e)
	{
		// Left mapped: it may have registered something before it threw.
		r.outcome = Failed;
		r.message = QString::fromUtf8(e.what());
	}
	catch (...)
	{
		r.outcome = Failed;
		r.message = QObject::tr("unknown error");
	}
	TransferActions::setLoadingPlugin(QString());

	return r;
}
