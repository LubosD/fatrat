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

#include "RestResources.h"

#include <QJsonArray>
#include <QJsonObject>

#include "python/PluginManifest.h"
#include "python/PluginRegistry.h"
#include "python/PluginRepository.h"

/* /plugins and /plugin-repositories.
 *
 * The same operations the desktop settings page offers, so a headless FatRat
 * is not a second-class one: a plugin's settings page comes down as the
 * `config` schema from its manifest, and the web UI renders it with the same
 * widget vocabulary the desktop does.
 *
 * Installing and refreshing can take seconds (git over the network). They run
 * on the HTTP thread that asked for them; PluginRepository locks.
 */

static QJsonObject manifestToJson(const PluginManifest& manifest, const QString& repository,
				  const QString& installedFrom)
{
	QJsonObject rv;

	if (!manifest.valid)
	{
		// Surfaced rather than hidden: a plugin that silently does not
		// appear is the worst kind.
		rv["name"] = manifest.directory.section('/', -1);
		rv["valid"] = false;
		rv["error"] = manifest.error;
		return rv;
	}

	rv["name"] = manifest.name;
	rv["valid"] = true;
	rv["version"] = manifest.version;
	rv["description"] = manifest.description;
	rv["license"] = manifest.license;
	rv["homepage"] = manifest.homepage;
	rv["api"] = manifest.apiVersion;
	rv["enabled"] = PluginRegistry::isPluginEnabled(manifest.name);
	// The settings key behind "enabled", for clients that drive settings
	// generically (the TUI does).
	rv["enabledKey"] = PluginRegistry::enabledKey(manifest.name);
	rv["config"] = manifest.config;

	// What is loaded right now. "draining": switched off or being replaced
	// or removed, but still held by transfers that were started from it.
	PluginRegistry::Status status = PluginRegistry::status(manifest.name);

	rv["state"] = status.draining ? "draining" : status.loaded
		? (status.error.isEmpty() ? "loaded" : "failed") : "unloaded";
	rv["activeTransfers"] = status.activeTransfers;
	rv["pendingReload"] = status.pendingReload;
	rv["pendingUninstall"] = status.pendingUninstall;
	if (!status.error.isEmpty())
		rv["loadError"] = status.error;

	rv["repository"] = repository;
	rv["source"] = installedFrom;

	QJsonArray permissions;

	foreach (const QString& permission, manifest.permissions)
		permissions << permission;

	rv["permissions"] = permissions;

	return rv;
}

static RestResult listPlugins(const RestRequest&)
{
	QList<PluginRepository::Installed> records = PluginRepository::installed();
	QJsonArray rv;

	foreach (const PluginManifest& manifest, PluginRegistry::installed())
	{
		QString repository, source;

		foreach (const PluginRepository::Installed& record, records)
		{
			if (record.plugin == manifest.name)
			{
				repository = record.repository;
				source = record.source;
				break;
			}
		}

		rv << manifestToJson(manifest, repository, source);
	}

	return RestResult::ok(rv);
}

static const PluginManifest findPlugin(const QString& name)
{
	foreach (const PluginManifest& manifest, PluginRegistry::installed())
	{
		if (manifest.valid && manifest.name == name)
			return manifest;
	}

	throw RestError(404, "plugin_not_found", QString("No such plugin: %1").arg(name));
}

static RestResult getPlugin(const RestRequest& req)
{
	return RestResult::ok(manifestToJson(findPlugin(req.pathParams["plugin"]), QString(), QString()));
}

static RestResult patchPlugin(const RestRequest& req)
{
	PluginManifest manifest = findPlugin(req.pathParams["plugin"]);
	QJsonObject body = req.jsonObject();

	for (QJsonObject::const_iterator it = body.begin(); it != body.end(); it++)
	{
		if (it.key() == "enabled")
			PluginRegistry::setPluginEnabled(manifest.name, RestApi::asBool(it.value(), "enabled"));
		else
			throw RestError(422, "unknown_property", QString("Unknown property: %1").arg(it.key()));
	}

	// Applied already (or draining, which the state says).
	return RestResult::ok(manifestToJson(findPlugin(manifest.name), QString(), QString()));
}

// Imports the plugin's files again: the development loop for a plugin used in place.
static RestResult reloadPlugin(const RestRequest& req)
{
	PluginManifest manifest = findPlugin(req.pathParams["plugin"]);

	PluginRegistry::reload(manifest.name);

	return RestResult::ok(manifestToJson(findPlugin(manifest.name), QString(), QString()));
}

static RestResult deletePlugin(const RestRequest& req)
{
	QString name = req.pathParams["plugin"];
	QString error;

	findPlugin(name);

	if (!PluginRegistry::uninstall(name, &error))
		throw RestError(500, "uninstall_failed", error);

	// 202 if transfers still use it: the files go when they are done.
	if (PluginRegistry::status(name).pendingUninstall)
	{
		QJsonObject rv;

		rv["plugin"] = name;
		rv["state"] = "draining";
		rv["activeTransfers"] = PluginRegistry::status(name).activeTransfers;

		return RestResult::accepted(rv);
	}

	return RestResult::noContent();
}

static RestResult installPlugin(const RestRequest& req)
{
	QJsonObject body = req.jsonObject();
	QString repository = RestApi::asString(body.value("repository"), "repository");
	QString plugin = RestApi::asString(body.value("plugin"), "plugin");
	bool withDependencies = body.contains("withDependencies")
		&& RestApi::asBool(body.value("withDependencies"), "withDependencies");
	QString error;

	// Packages from PyPI are code the user did not pick, so the client has
	// to say yes to them specifically, having been told what they are.
	QStringList dependencies = PluginRepository::dependenciesOf(repository, plugin);

	if (!dependencies.isEmpty() && !withDependencies)
	{
		throw RestError(422, "dependencies_required",
				QString("%1 needs these packages from PyPI: %2")
					.arg(plugin).arg(dependencies.join(", ")));
	}

	if (!PluginRepository::install(repository, plugin, &error, withDependencies))
		throw RestError(422, "install_failed", error);

	QJsonObject rv;

	rv["plugin"] = plugin;
	rv["repository"] = repository;

	QJsonArray installedDependencies;

	foreach (const QString& dependency, dependencies)
		installedDependencies << dependency;

	rv["dependencies"] = installedDependencies;
	// Loads it now, or replaces the running version once its transfers end.
	PluginRegistry::reload(plugin);

	PluginRegistry::Status status = PluginRegistry::status(plugin);

	rv["state"] = status.draining ? "draining" : status.loaded
		? (status.error.isEmpty() ? "loaded" : "failed") : "unloaded";
	rv["activeTransfers"] = status.activeTransfers;
	rv["pendingReload"] = status.pendingReload;
	if (!status.error.isEmpty())
		rv["loadError"] = status.error;
	// Nothing needs a restart any more; kept for clients written before.
	rv["restartRequired"] = false;

	return RestResult::created(rv, QString("/plugins/%1").arg(plugin));
}

static RestResult listAvailable(const RestRequest& req)
{
	bool updatesOnly = req.queryBool("updates");
	QList<PluginRepository::Available> entries = updatesOnly
		? PluginRepository::updates() : PluginRepository::available();
	QJsonArray rv;

	foreach (const PluginRepository::Available& entry, entries)
	{
		QJsonObject o;

		o["name"] = entry.plugin.name;
		o["version"] = entry.plugin.version;
		o["description"] = entry.plugin.description;
		o["repository"] = entry.repository;
		o["installedVersion"] = entry.installedVersion;

		QJsonArray dependencies;

		foreach (const QString& dependency, PluginRepository::dependenciesOf(entry.repository, entry.plugin.name))
			dependencies << dependency;

		o["dependencies"] = dependencies;

		rv << o;
	}

	return RestResult::ok(rv);
}

static QJsonObject repositoryToJson(const PluginRepository::Repository& repository)
{
	QJsonObject rv;

	rv["id"] = repository.id;
	rv["source"] = repository.source;
	rv["local"] = repository.local;
	rv["name"] = repository.name;
	rv["description"] = repository.description;
	rv["revision"] = repository.revision;

	return rv;
}

static RestResult listRepositories(const RestRequest&)
{
	QJsonArray rv;

	foreach (const PluginRepository::Repository& repository, PluginRepository::repositories())
		rv << repositoryToJson(repository);

	return RestResult::ok(rv);
}

static RestResult addRepository(const RestRequest& req)
{
	QJsonObject body = req.jsonObject();
	QString source = RestApi::asString(body.value("source"), "source");
	QString error;

	if (!PluginRepository::addRepository(source, &error))
		throw RestError(422, "repository_rejected", error);

	QList<PluginRepository::Repository> repositories = PluginRepository::repositories();
	QJsonObject added = repositoryToJson(repositories.last());

	return RestResult::created(added, QString("/plugin-repositories/%1").arg(added["id"].toString()));
}

static RestResult deleteRepository(const RestRequest& req)
{
	QString error;

	if (!PluginRepository::removeRepository(req.pathParams["repository"], &error))
		throw RestError(404, "repository_not_found", error);

	return RestResult::noContent();
}

static RestResult refreshRepositories(const RestRequest& req)
{
	QJsonObject body = req.body.isEmpty() ? QJsonObject() : req.jsonObject();
	QString id = body.contains("repository")
		? RestApi::asString(body.value("repository"), "repository") : QString();
	QString error;

	if (!PluginRepository::refresh(id, &error))
		throw RestError(422, "refresh_failed", error);

	return listRepositories(req);
}

void RestResources::registerPlugins()
{
	RestApi::registerRoute("GET", "/plugins", listPlugins);
	RestApi::registerRoute("POST", "/plugins", installPlugin);
	RestApi::registerRoute("GET", "/plugins/available", listAvailable);
	RestApi::registerRoute("GET", "/plugins/:plugin", getPlugin);
	RestApi::registerRoute("PATCH", "/plugins/:plugin", patchPlugin);
	RestApi::registerRoute("DELETE", "/plugins/:plugin", deletePlugin);
	RestApi::registerRoute("POST", "/plugins/:plugin/reload", reloadPlugin);

	RestApi::registerRoute("GET", "/plugin-repositories", listRepositories);
	RestApi::registerRoute("POST", "/plugin-repositories", addRepository);
	RestApi::registerRoute("POST", "/plugin-repositories/refresh", refreshRepositories);
	RestApi::registerRoute("DELETE", "/plugin-repositories/:repository", deleteRepository);
}
