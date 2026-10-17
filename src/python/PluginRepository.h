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

#ifndef PLUGINREPOSITORY_H
#define PLUGINREPOSITORY_H

#include "config.h"
#ifndef WITH_PYPLUGINS
#	error This file is not supposed to be included!
#endif

#include <QList>
#include <QString>

#include "PluginManifest.h"

/* Where plugins come from.
 *
 * FatRat has no plugin index. A user adds a repository -- a git URL, or a
 * directory -- and installs individual plugins from it. What was installed,
 * from where, and at which revision is recorded in repos.json, so an update
 * is something the user asks for rather than something that happens to them.
 *
 * A local directory is used in place (the plugin is symlinked), which is also
 * how you develop a plugin: edit, reload, done.
 *
 * Nothing here runs plugin code or needs the interpreter. Every entry point
 * takes a lock: the desktop UI calls these from the main thread, the REST API
 * from its HTTP threads, and a git fetch is slow enough that both will
 * genuinely overlap.
 */
class PluginRepository
{
public:
	struct Repository
	{
		Repository() : local(false) {}

		QString id;        // slug, also the directory name under repos/
		QString source;    // git URL, or a local path
		bool local;        // used in place rather than cloned
		QString name, description;
		QString revision;  // the commit FatRat last fetched (git only)
	};

	struct Installed
	{
		QString plugin;     // the installed plugin's name
		QString repository; // the repository id it came from
		QString version;    // version at install time
		QString revision;   // repository commit at install time
		QString source;     // the entry's source within the repository
	};

	struct Available
	{
		RepositoryPlugin plugin;
		QString repository;   // repository id
		QString installedVersion; // empty when not installed
	};

	// ~/.local/share/fatrat/plugins, unless a test points it elsewhere.
	static QString root();
	static void setRoot(const QString& path);

	static QString installedPath();
	static QString repositoriesPath();

	static QList<Repository> repositories();
	static QList<Installed> installed();

	// Everything the configured repositories offer, with what is installed
	// of it marked. Reads the fetched copies; it does not fetch.
	static QList<Available> available();

	// Fetch the repository and read its manifest. For a git source this is
	// a clone or a fetch; for a local one it just re-reads the directory.
	static bool addRepository(const QString& source, QString* error);
	static bool removeRepository(const QString& id, QString* error);
	// Re-fetch one repository, or all of them when id is empty.
	static bool refresh(const QString& id, QString* error);

	/* Installs, or updates, one plugin.
	 *
	 * A plugin that declares `dependencies` is refused unless
	 * `withDependencies` is true: those packages are fetched from PyPI with
	 * pip and are code the user did not choose, so asking is the whole
	 * point. Use dependenciesOf() to find out what would be installed.
	 */
	static bool install(const QString& repositoryId, const QString& plugin, QString* error,
			    bool withDependencies = false);

	// What `plugin` would pull in from PyPI, read from the repository copy.
	static QStringList dependenciesOf(const QString& repositoryId, const QString& plugin);

	// Where a plugin's dependencies are installed, and the interpreter used
	// to install them.
	static QString vendorPath(const QString& plugin);
	static QString pythonExecutable();
	static bool uninstall(const QString& plugin, QString* error);

	// Plugins whose repository now offers a version other than the one that
	// was installed.
	static QList<Available> updates();

	// Whether a git source can be used at all on this machine.
	static bool gitAvailable();

	// True when `source` names a git repository rather than a directory.
	static bool looksLikeGitSource(const QString& source);
private:
	static QString slugFor(const QString& source);
	static bool runGit(const QStringList& arguments, const QString& workingDirectory,
			   QString* output, QString* error);
	static bool copyDirectory(const QString& from, const QString& to, QString* error);
	static void load();
	static void save();
	// uninstall() with the lock already held (install() rolls back with it).
	static bool uninstallLocked(const QString& plugin, QString* error);
};

#endif // PLUGINREPOSITORY_H
