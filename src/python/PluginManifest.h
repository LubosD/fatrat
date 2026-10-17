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

#ifndef PLUGINMANIFEST_H
#define PLUGINMANIFEST_H

#include "config.h"
#ifndef WITH_PYPLUGINS
#	error This file is not supposed to be included!
#endif

#include <QJsonArray>
#include <QList>
#include <QString>
#include <QStringList>

/* A plugin's plugin.json, and the repository manifest that points at it.
 *
 * Parsing lives apart from the registry because it has nothing to do with
 * Python: it is reading JSON off disk, and it has to be testable without an
 * interpreter, an engine registry or a running FatRat.
 */
class PluginManifest
{
public:
	PluginManifest() : valid(false) {}

	bool valid;
	QString directory;
	QString name, version, description, entry, license, homepage, apiVersion, minFatRat;
	QStringList permissions;
	// PyPI requirements the plugin needs. Installing them is a separate,
	// explicitly confirmed step: it runs pip, which fetches and executes
	// code the user did not pick themselves.
	QStringList dependencies;
	// The plugin's settings page, declared as data so that every FatRat
	// interface can render the same thing.
	QJsonArray config;
	QString error; // why it was rejected, when valid is false

	// Read one plugin.json. Never throws: an unreadable or unsupported
	// manifest comes back with valid == false and an explanation.
	// `apiVersion` is the plugin API this FatRat implements.
	static PluginManifest read(const QString& directory, const QString& apiVersion);

	// Every plugin directory under `root`, manifests and all.
	static QList<PluginManifest> readAll(const QString& root, const QString& apiVersion);
};

/* One entry of a repository's fatrat-plugins.json. */
struct RepositoryPlugin
{
	QString name, description, source, version;
};

/* A repository manifest: what a user adds by URL or path. */
class RepositoryManifest
{
public:
	RepositoryManifest() : valid(false) {}

	bool valid;
	QString name, description, homepage;
	QList<RepositoryPlugin> plugins;
	QString error;

	static RepositoryManifest read(const QString& directory);
	static RepositoryManifest parse(const QByteArray& json);
};

#endif // PLUGINMANIFEST_H
