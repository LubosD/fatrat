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
	struct Manifest
	{
		Manifest() : valid(false) {}

		bool valid;
		QString directory;
		QString name, version, description, entry, license, homepage, apiVersion, minFatRat;
		QStringList permissions;
		QString error; // why it was rejected, when valid is false
	};

	struct LoadedPlugin
	{
		Manifest manifest;
		QStringList engines; // identifiers registered by this plugin
		QString error;       // import failure, if any
	};

	// Import every enabled plugin and register its engines. Called once at
	// startup, after the interpreter is up.
	static void globalInit();
	static void globalExit();

	// Read one plugin.json. Never throws: an unreadable or unsupported
	// manifest comes back with valid == false and an explanation.
	static Manifest readManifest(const QString& directory);

	// Manifests of everything in the installed-plugins directory.
	static QList<Manifest> installed();

	// What globalInit() made of them.
	static QList<LoadedPlugin> loaded();

	// Plugins the user has switched off, by name.
	static QStringList disabledPlugins();
	static void setPluginEnabled(const QString& name, bool enabled);
	static bool isPluginEnabled(const QString& name);

	// Import one plugin directory and register its engines. Used by
	// globalInit() and, later, right after an install.
	static LoadedPlugin loadPlugin(const Manifest& manifest);
};

#endif // PLUGINREGISTRY_H
