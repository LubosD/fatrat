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

#ifndef PLUGINREGISTRY_H
#define PLUGINREGISTRY_H

#include "config.h"
#ifndef WITH_PYPLUGINS
#	error This file is not supposed to be included!
#endif

#include <QList>
#include <QString>
#include <QStringList>

#include "PluginManifest.h"

/* What is installed, and what of it is loaded.
 *
 * A plugin is a directory with a plugin.json manifest. Discovery reads the
 * manifests, imports the entry module of each enabled plugin, and registers
 * what the module's decorators declared as ordinary FatRat transfer engines
 * -- from there on a Python plugin is indistinguishable from a built-in one.
 *
 * Where those directories come from is PluginRepository's business
 * (installed from a user-added repository, or symlinked for development).
 */
class PluginRegistry
{
public:
	// The manifest type lives in PluginManifest.h: parsing JSON has
	// nothing to do with the interpreter, and is tested without one.
	typedef PluginManifest Manifest;

	struct LoadedPlugin
	{
		Manifest manifest;
		QStringList engines; // identifiers registered by this plugin
		QString error;       // import failure, if any
		bool draining;       // withdrawn, waiting for its last transfers to end

		LoadedPlugin() : draining(false) {}
	};

	// What a UI needs to know about one plugin right now.
	struct Status
	{
		Status() : loaded(false), draining(false), pendingReload(false), pendingUninstall(false),
			activeTransfers(0) {}

		bool loaded;
		bool draining;
		bool pendingReload;    // will be loaded again once the transfers are done
		bool pendingUninstall; // will be removed once the transfers are done
		int activeTransfers;   // transfers still using it
		QString error;         // why it failed to load, if it did
	};

	// Import every enabled plugin and register its engines. Called once at
	// startup, after the interpreter is up.
	static void globalInit();
	static void globalExit();

	// The plugin API version this FatRat implements.
	static QString apiVersion();

	// Manifests of everything in the installed-plugins directory.
	static QList<Manifest> installed();

	// What globalInit() made of them.
	static QList<LoadedPlugin> loaded();

	// Whether a plugin is loaded. Stored as an ordinary settings key, so
	// every interface can read and write it. setPluginEnabled() takes effect
	// at once (see sync()).
	static QString enabledKey(const QString& name);
	static void setPluginEnabled(const QString& name, bool enabled);
	static bool isPluginEnabled(const QString& name);

	// Import one plugin directory and register its engines. Used by
	// globalInit() and by sync().
	static LoadedPlugin loadPlugin(const Manifest& manifest);

	/* Plugins come and go while FatRat runs.
	 *
	 * Loading is immediate. Unloading is not, if transfers created from the
	 * plugin still exist: the plugin is then "draining" -- no new transfer
	 * can be started from it (it vanishes from the engine lists and from
	 * URL matching), the existing ones carry on, and it is unloaded when
	 * the last of them is gone. Enabling it again in the meantime just
	 * cancels the drain.
	 *
	 * All of these may be called from any thread; the work is done on the
	 * main thread.
	 */

	// Make what is loaded match what is installed and enabled, for one
	// plugin: load it, or start unloading it. Call after the plugin was
	// installed, upgraded, or enabled or disabled.
	static void sync(const QString& name);

	// sync() for every installed plugin. For settings that were written
	// as plain keys (the web UI and the TUI do), which nobody told us about.
	static void syncAll();

	// Unload and load again (picking up edited code), once idle.
	static void reload(const QString& name);

	// PluginRepository::uninstall(), but the files stay until the plugin's
	// transfers are done with it. False only if removing the files failed.
	static bool uninstall(const QString& name, QString* error);

	static Status status(const QString& name);

	// Unload the draining plugins that have no transfers left. Called when
	// a transfer ends.
	static void finishDrains();

	// Tell the plugins that implement on_config_saved() that settings were
	// saved. Safe to call when no interpreter is running.
	static void notifyConfigSaved();

	// One line in the log for someone upgrading from the Java extensions,
	// whose jars are still sitting in their profile doing nothing. Needs no
	// interpreter.
	static void reportLegacyJavaPlugins();
};

#endif // PLUGINREGISTRY_H
