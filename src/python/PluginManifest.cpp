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

#include "PluginManifest.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>

PluginManifest PluginManifest::read(const QString& directory, const QString& apiVersion)
{
	PluginManifest manifest;
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
	manifest.config = root.value(QLatin1String("config")).toArray();

	QJsonArray permissions = root.value(QLatin1String("permissions")).toArray();
	for (QJsonArray::const_iterator it = permissions.begin(); it != permissions.end(); it++)
		manifest.permissions << (*it).toString();

	QJsonArray dependencies = root.value(QLatin1String("dependencies")).toArray();
	for (QJsonArray::const_iterator it = dependencies.begin(); it != dependencies.end(); it++)
	{
		QString requirement = (*it).toString().trimmed();

		// Nothing clever: a requirement FatRat cannot read is one it will
		// not hand to pip.
		if (!requirement.isEmpty())
			manifest.dependencies << requirement;
	}

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
		manifest.apiVersion = apiVersion;
	if (manifest.apiVersion != apiVersion)
	{
		manifest.error = QObject::tr("%1 needs plugin API %2, this FatRat has %3")
			.arg(manifest.name).arg(manifest.apiVersion).arg(apiVersion);
		return manifest;
	}

	manifest.valid = true;
	return manifest;
}

QList<PluginManifest> PluginManifest::readAll(const QString& root, const QString& apiVersion)
{
	QList<PluginManifest> rv;
	QDir dir(root);

	if (!dir.exists())
		return rv;

	foreach (const QString& name, dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
		rv << read(dir.filePath(name), apiVersion);

	return rv;
}

RepositoryManifest RepositoryManifest::read(const QString& directory)
{
	QFile file(QDir(directory).filePath(QLatin1String("fatrat-plugins.json")));

	if (!file.open(QIODevice::ReadOnly))
	{
		RepositoryManifest manifest;

		manifest.error = QObject::tr("no fatrat-plugins.json in %1").arg(directory);
		return manifest;
	}

	return parse(file.readAll());
}

RepositoryManifest RepositoryManifest::parse(const QByteArray& json)
{
	RepositoryManifest manifest;
	QJsonParseError parseError;
	QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);

	if (doc.isNull() || !doc.isObject())
	{
		manifest.error = QObject::tr("fatrat-plugins.json is not valid JSON: %1").arg(parseError.errorString());
		return manifest;
	}

	QJsonObject root = doc.object();

	manifest.name = root.value(QLatin1String("name")).toString();
	manifest.description = root.value(QLatin1String("description")).toString();
	manifest.homepage = root.value(QLatin1String("homepage")).toString();

	QJsonArray plugins = root.value(QLatin1String("plugins")).toArray();

	for (QJsonArray::const_iterator it = plugins.begin(); it != plugins.end(); it++)
	{
		QJsonObject entry = (*it).toObject();
		RepositoryPlugin plugin;

		plugin.name = entry.value(QLatin1String("name")).toString();
		plugin.description = entry.value(QLatin1String("description")).toString();
		plugin.source = entry.value(QLatin1String("source")).toString();
		plugin.version = entry.value(QLatin1String("version")).toString();

		if (plugin.name.isEmpty())
			continue; // an entry FatRat cannot install is not worth listing

		if (plugin.source.isEmpty())
			plugin.source = QLatin1String("./") + plugin.name;

		manifest.plugins << plugin;
	}

	if (manifest.name.isEmpty())
	{
		manifest.error = QObject::tr("the repository manifest has no name");
		return manifest;
	}

	manifest.valid = true;
	return manifest;
}
