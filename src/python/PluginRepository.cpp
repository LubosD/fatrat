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

#include "PluginRepository.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QMutex>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

#include <cstdlib>
#include <QtDebug>

// The plugin API version manifests are checked against. Kept here rather than
// pulled from PluginRegistry so that this unit needs no interpreter.
static const QLatin1String API_VERSION("1");

// Recursive: the public functions call one another (install() loads, refresh()
// saves), and all of them lock.
static QRecursiveMutex s_mutex;
static QString s_root;
static bool s_loaded = false;
static QList<PluginRepository::Repository> s_repositories;
static QList<PluginRepository::Installed> s_installed;

QString PluginRepository::root()
{
	if (!s_root.isEmpty())
		return s_root;

	return QDir::homePath() + QLatin1String(USER_PROFILE_PATH "/plugins");
}

void PluginRepository::setRoot(const QString& path)
{
	QMutexLocker locker(&s_mutex);

	s_root = path;
	s_loaded = false;
	s_repositories.clear();
	s_installed.clear();
}

QString PluginRepository::installedPath()
{
	return QDir(root()).filePath(QLatin1String("installed"));
}

QString PluginRepository::repositoriesPath()
{
	return QDir(root()).filePath(QLatin1String("repos"));
}

static QString statePath()
{
	return QDir(PluginRepository::root()).filePath(QLatin1String("repos.json"));
}

void PluginRepository::load()
{
	if (s_loaded)
		return;

	s_loaded = true;
	s_repositories.clear();
	s_installed.clear();

	QFile file(statePath());

	if (!file.open(QIODevice::ReadOnly))
		return; // nothing configured yet, which is the default state

	QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
	QJsonArray repositories = root.value(QLatin1String("repositories")).toArray();

	for (QJsonArray::const_iterator it = repositories.begin(); it != repositories.end(); it++)
	{
		QJsonObject entry = (*it).toObject();
		Repository repository;

		repository.id = entry.value(QLatin1String("id")).toString();
		repository.source = entry.value(QLatin1String("source")).toString();
		repository.local = entry.value(QLatin1String("local")).toBool();
		repository.name = entry.value(QLatin1String("name")).toString();
		repository.description = entry.value(QLatin1String("description")).toString();
		repository.revision = entry.value(QLatin1String("revision")).toString();

		if (!repository.id.isEmpty())
			s_repositories << repository;
	}

	QJsonArray installed = root.value(QLatin1String("installed")).toArray();

	for (QJsonArray::const_iterator it = installed.begin(); it != installed.end(); it++)
	{
		QJsonObject entry = (*it).toObject();
		Installed record;

		record.plugin = entry.value(QLatin1String("plugin")).toString();
		record.repository = entry.value(QLatin1String("repository")).toString();
		record.version = entry.value(QLatin1String("version")).toString();
		record.revision = entry.value(QLatin1String("revision")).toString();
		record.source = entry.value(QLatin1String("source")).toString();

		if (!record.plugin.isEmpty())
			s_installed << record;
	}
}

void PluginRepository::save()
{
	QJsonArray repositories, installed;

	foreach (const Repository& repository, s_repositories)
	{
		QJsonObject entry;

		entry[QLatin1String("id")] = repository.id;
		entry[QLatin1String("source")] = repository.source;
		entry[QLatin1String("local")] = repository.local;
		entry[QLatin1String("name")] = repository.name;
		entry[QLatin1String("description")] = repository.description;
		entry[QLatin1String("revision")] = repository.revision;

		repositories << entry;
	}

	foreach (const Installed& record, s_installed)
	{
		QJsonObject entry;

		entry[QLatin1String("plugin")] = record.plugin;
		entry[QLatin1String("repository")] = record.repository;
		entry[QLatin1String("version")] = record.version;
		entry[QLatin1String("revision")] = record.revision;
		entry[QLatin1String("source")] = record.source;

		installed << entry;
	}

	QJsonObject root;

	root[QLatin1String("schema")] = 1;
	root[QLatin1String("repositories")] = repositories;
	root[QLatin1String("installed")] = installed;

	QDir().mkpath(PluginRepository::root());

	QFile file(statePath());

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
	{
		qDebug() << "Cannot write" << statePath() << file.errorString();
		return;
	}

	file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

QList<PluginRepository::Repository> PluginRepository::repositories()
{
	QMutexLocker locker(&s_mutex);

	load();
	return s_repositories;
}

QList<PluginRepository::Installed> PluginRepository::installed()
{
	QMutexLocker locker(&s_mutex);

	load();
	return s_installed;
}

bool PluginRepository::looksLikeGitSource(const QString& source)
{
	static const QRegularExpression scheme("^(https?|git|ssh|file)://");
	static const QRegularExpression scp("^[^/]+@[^/]+:");

	if (scheme.match(source).hasMatch() || scp.match(source).hasMatch())
		return true;

	// A bare path that is not a directory is nothing we can use, but treat
	// a ".git" path as a git source so a local bare repository works too.
	return source.endsWith(QLatin1String(".git"));
}

bool PluginRepository::gitAvailable()
{
	return !QStandardPaths::findExecutable(QLatin1String("git")).isEmpty();
}

QString PluginRepository::slugFor(const QString& source)
{
	QString name = source;

	name.remove(QRegularExpression("\\.git$"));
	name.remove(QRegularExpression("/+$"));

	int slash = name.lastIndexOf('/');
	if (slash >= 0)
		name = name.mid(slash + 1);

	name.replace(QRegularExpression("[^A-Za-z0-9._-]"), "-");

	if (name.isEmpty())
		name = QLatin1String("repository");

	// Keep ids unique: two "plugins" repositories from different hosts are
	// not the same repository.
	QString candidate = name;
	int suffix = 2;

	forever
	{
		bool taken = false;

		foreach (const Repository& repository, s_repositories)
		{
			if (repository.id == candidate)
			{
				taken = true;
				break;
			}
		}

		if (!taken)
			return candidate;

		candidate = QString("%1-%2").arg(name).arg(suffix++);
	}
}

bool PluginRepository::runGit(const QStringList& arguments, const QString& workingDirectory,
			      QString* output, QString* error)
{
	QProcess process;

	if (!workingDirectory.isEmpty())
		process.setWorkingDirectory(workingDirectory);

	process.start(QLatin1String("git"), arguments);

	if (!process.waitForStarted(10000))
	{
		if (error)
			*error = QObject::tr("git could not be started; install git to use git repositories");
		return false;
	}

	// Generous, because a first clone over a slow link is not a failure.
	if (!process.waitForFinished(300000))
	{
		process.kill();
		if (error)
			*error = QObject::tr("git timed out");
		return false;
	}

	QString stdOut = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
	QString stdErr = QString::fromUtf8(process.readAllStandardError()).trimmed();

	if (output)
		*output = stdOut;

	if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
	{
		if (error)
			*error = stdErr.isEmpty() ? QObject::tr("git failed") : stdErr;
		return false;
	}

	return true;
}

bool PluginRepository::copyDirectory(const QString& from, const QString& to, QString* error)
{
	QDir source(from);

	if (!source.exists())
	{
		if (error)
			*error = QObject::tr("%1 does not exist").arg(from);
		return false;
	}

	QDir().mkpath(to);

	foreach (const QFileInfo& info, source.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot))
	{
		QString target = QDir(to).filePath(info.fileName());

		if (info.isDir())
		{
			if (!copyDirectory(info.absoluteFilePath(), target, error))
				return false;
		}
		else
		{
			QFile::remove(target);

			if (!QFile::copy(info.absoluteFilePath(), target))
			{
				if (error)
					*error = QObject::tr("failed to copy %1").arg(info.absoluteFilePath());
				return false;
			}
		}
	}

	return true;
}

bool PluginRepository::addRepository(const QString& source, QString* error)
{
	QMutexLocker locker(&s_mutex);

	load();

	QString cleaned = source.trimmed();

	if (cleaned.isEmpty())
	{
		if (error)
			*error = QObject::tr("no repository given");
		return false;
	}

	foreach (const Repository& existing, s_repositories)
	{
		if (existing.source == cleaned)
		{
			if (error)
				*error = QObject::tr("that repository has already been added");
			return false;
		}
	}

	Repository repository;

	repository.source = cleaned;
	repository.local = !looksLikeGitSource(cleaned);
	repository.id = slugFor(cleaned);

	if (repository.local)
	{
		QFileInfo info(cleaned);

		if (!info.isDir())
		{
			if (error)
				*error = QObject::tr("%1 is neither a directory nor a git URL").arg(cleaned);
			return false;
		}

		repository.source = info.absoluteFilePath();
	}
	else
	{
		if (!gitAvailable())
		{
			if (error)
				*error = QObject::tr("git is not installed, so git repositories cannot be used");
			return false;
		}

		QDir().mkpath(repositoriesPath());

		QString target = QDir(repositoriesPath()).filePath(repository.id);

		QDir(target).removeRecursively();

		if (!runGit(QStringList() << "clone" << "--depth" << "1" << cleaned << target,
			    repositoriesPath(), 0, error))
			return false;

		QString revision;

		if (runGit(QStringList() << "rev-parse" << "HEAD", target, &revision, 0))
			repository.revision = revision;
	}

	// Reading the manifest is also the check that this is a plugin
	// repository at all, rather than any old directory.
	QString directory = repository.local ? repository.source
					     : QDir(repositoriesPath()).filePath(repository.id);
	RepositoryManifest manifest = RepositoryManifest::read(directory);

	if (!manifest.valid)
	{
		if (!repository.local)
			QDir(directory).removeRecursively();

		if (error)
			*error = manifest.error.isEmpty()
				? QObject::tr("%1 is not a FatRat plugin repository").arg(cleaned)
				: manifest.error;
		return false;
	}

	repository.name = manifest.name;
	repository.description = manifest.description;

	s_repositories << repository;
	save();

	return true;
}

bool PluginRepository::removeRepository(const QString& id, QString* error)
{
	QMutexLocker locker(&s_mutex);

	load();

	for (int i = 0; i < s_repositories.size(); i++)
	{
		if (s_repositories[i].id != id)
			continue;

		if (!s_repositories[i].local)
			QDir(QDir(repositoriesPath()).filePath(id)).removeRecursively();

		s_repositories.removeAt(i);
		save();

		return true;
	}

	if (error)
		*error = QObject::tr("no such repository: %1").arg(id);

	return false;
}

bool PluginRepository::refresh(const QString& id, QString* error)
{
	QMutexLocker locker(&s_mutex);

	load();

	bool any = false;

	for (int i = 0; i < s_repositories.size(); i++)
	{
		Repository& repository = s_repositories[i];

		if (!id.isEmpty() && repository.id != id)
			continue;

		any = true;

		QString directory = repository.local ? repository.source
						     : QDir(repositoriesPath()).filePath(repository.id);

		if (!repository.local)
		{
			if (!runGit(QStringList() << "fetch" << "--depth" << "1" << "origin", directory, 0, error))
				return false;
			if (!runGit(QStringList() << "reset" << "--hard" << "FETCH_HEAD", directory, 0, error))
				return false;

			QString revision;

			if (runGit(QStringList() << "rev-parse" << "HEAD", directory, &revision, 0))
				repository.revision = revision;
		}

		RepositoryManifest manifest = RepositoryManifest::read(directory);

		if (manifest.valid)
		{
			repository.name = manifest.name;
			repository.description = manifest.description;
		}
	}

	if (!any && !id.isEmpty())
	{
		if (error)
			*error = QObject::tr("no such repository: %1").arg(id);
		return false;
	}

	save();

	return true;
}

QList<PluginRepository::Available> PluginRepository::available()
{
	QMutexLocker locker(&s_mutex);

	load();

	QList<Available> rv;

	foreach (const Repository& repository, s_repositories)
	{
		QString directory = repository.local ? repository.source
						     : QDir(repositoriesPath()).filePath(repository.id);
		RepositoryManifest manifest = RepositoryManifest::read(directory);

		if (!manifest.valid)
			continue;

		foreach (const RepositoryPlugin& plugin, manifest.plugins)
		{
			Available entry;

			entry.plugin = plugin;
			entry.repository = repository.id;

			// The version in the repository manifest is optional; the
			// plugin's own manifest is authoritative.
			PluginManifest pluginManifest = PluginManifest::read(
				QDir(directory).filePath(plugin.source), API_VERSION);

			if (pluginManifest.valid && !pluginManifest.version.isEmpty())
				entry.plugin.version = pluginManifest.version;
			if (entry.plugin.description.isEmpty())
				entry.plugin.description = pluginManifest.description;

			foreach (const Installed& record, s_installed)
			{
				if (record.plugin == plugin.name)
				{
					entry.installedVersion = record.version;
					break;
				}
			}

			rv << entry;
		}
	}

	return rv;
}

QString PluginRepository::pythonExecutable()
{
	// An override, for a build whose interpreter has no pip of its own (and
	// for tests): whatever runs pip must be the same Python the plugins
	// will be imported by, or the packages land where nothing reads them.
	if (const char* env = getenv("FATRAT_PYTHON_EXECUTABLE"))
	{
		if (*env)
			return QString::fromLocal8Bit(env);
	}

	// Otherwise the interpreter FatRat was built against.
#ifdef FATRAT_PYTHON_EXECUTABLE
	QString configured = QLatin1String(FATRAT_PYTHON_EXECUTABLE);

	if (!configured.isEmpty())
		return configured;
#endif
	return QLatin1String("python3");
}

QString PluginRepository::vendorPath(const QString& plugin)
{
	return QDir(installedPath()).filePath(plugin + QLatin1String("/_vendor"));
}

QStringList PluginRepository::dependenciesOf(const QString& repositoryId, const QString& plugin)
{
	QMutexLocker locker(&s_mutex);

	load();

	foreach (const Repository& repository, s_repositories)
	{
		if (repository.id != repositoryId)
			continue;

		QString directory = repository.local ? repository.source
						     : QDir(repositoriesPath()).filePath(repository.id);
		RepositoryManifest manifest = RepositoryManifest::read(directory);

		foreach (const RepositoryPlugin& entry, manifest.plugins)
		{
			if (entry.name != plugin)
				continue;

			return PluginManifest::read(QDir(directory).filePath(entry.source),
						    API_VERSION).dependencies;
		}
	}

	return QStringList();
}

/* Fetches a plugin's requirements into its own _vendor directory.
 *
 * Per plugin rather than into the user's site-packages: two plugins may want
 * incompatible versions, and removing a plugin should take its dependencies
 * with it.
 */
static bool runPip(const QString& plugin, const QStringList& requirements, QString* error)
{
	QString target = PluginRepository::vendorPath(plugin);
	QStringList arguments;

	QDir().mkpath(target);

	arguments << "-m" << "pip" << "install" << "--no-input" << "--disable-pip-version-check"
		  << "--target" << target << requirements;

	QProcess process;

	process.start(PluginRepository::pythonExecutable(), arguments);

	if (!process.waitForStarted(10000))
	{
		if (error)
			*error = QObject::tr("could not run %1").arg(PluginRepository::pythonExecutable());
		return false;
	}

	if (!process.waitForFinished(600000))
	{
		process.kill();
		if (error)
			*error = QObject::tr("pip timed out");
		return false;
	}

	if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
	{
		QString message = QString::fromUtf8(process.readAllStandardError()).trimmed();

		if (message.isEmpty())
			message = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
		if (error)
			*error = message.isEmpty() ? QObject::tr("pip failed") : message;

		return false;
	}

	return true;
}

bool PluginRepository::install(const QString& repositoryId, const QString& plugin, QString* error,
			       bool withDependencies)
{
	QMutexLocker locker(&s_mutex);

	load();

	const Repository* repository = 0;

	foreach (const Repository& candidate, s_repositories)
	{
		if (candidate.id == repositoryId)
		{
			repository = &candidate;
			break;
		}
	}

	if (!repository)
	{
		if (error)
			*error = QObject::tr("no such repository: %1").arg(repositoryId);
		return false;
	}

	QString directory = repository->local ? repository->source
					      : QDir(repositoriesPath()).filePath(repository->id);
	RepositoryManifest manifest = RepositoryManifest::read(directory);

	if (!manifest.valid)
	{
		if (error)
			*error = manifest.error;
		return false;
	}

	RepositoryPlugin entry;
	bool found = false;

	foreach (const RepositoryPlugin& candidate, manifest.plugins)
	{
		if (candidate.name == plugin)
		{
			entry = candidate;
			found = true;
			break;
		}
	}

	if (!found)
	{
		if (error)
			*error = QObject::tr("%1 does not offer a plugin called %2")
				.arg(repository->name.isEmpty() ? repository->id : repository->name).arg(plugin);
		return false;
	}

	QString pluginSource;

	if (looksLikeGitSource(entry.source))
	{
		// A repository may point at a plugin that lives in its own git
		// repository; clone that into the repository's directory.
		if (!gitAvailable())
		{
			if (error)
				*error = QObject::tr("git is not installed, so this plugin cannot be installed");
			return false;
		}

		pluginSource = QDir(directory).filePath(QLatin1String(".external/") + plugin);

		QDir(pluginSource).removeRecursively();
		QDir().mkpath(QFileInfo(pluginSource).path());

		if (!runGit(QStringList() << "clone" << "--depth" << "1" << entry.source << pluginSource,
			    QString(), 0, error))
			return false;
	}
	else
		pluginSource = QDir(directory).filePath(entry.source);

	PluginManifest pluginManifest = PluginManifest::read(pluginSource, API_VERSION);

	if (!pluginManifest.valid)
	{
		if (error)
			*error = pluginManifest.error;
		return false;
	}

	// Dependencies are code from PyPI that the user did not choose, so they
	// are never installed as a side effect of installing a plugin.
	if (!pluginManifest.dependencies.isEmpty() && !withDependencies)
	{
		if (error)
			*error = QObject::tr("%1 needs these packages from PyPI: %2")
				.arg(plugin).arg(pluginManifest.dependencies.join(QLatin1String(", ")));
		return false;
	}

	QDir().mkpath(installedPath());

	QString target = QDir(installedPath()).filePath(plugin);
	QFileInfo targetInfo(target);

	// Replace whatever was there, symlink or directory.
	if (targetInfo.isSymLink() || targetInfo.isFile())
		QFile::remove(target);
	else if (targetInfo.isDir())
		QDir(target).removeRecursively();

	if (repository->local)
	{
		// Used in place: editing the plugin and restarting FatRat is the
		// whole development cycle.
		if (!QFile::link(QFileInfo(pluginSource).absoluteFilePath(), target))
		{
			if (error)
				*error = QObject::tr("failed to link %1 to %2").arg(pluginSource).arg(target);
			return false;
		}
	}
	else if (!copyDirectory(pluginSource, target, error))
		return false;

	if (!pluginManifest.dependencies.isEmpty() && !runPip(plugin, pluginManifest.dependencies, error))
	{
		// A plugin without its dependencies would only fail on import, in
		// a way the user cannot act on.
		uninstallLocked(plugin, 0);
		return false;
	}

	Installed record;

	record.plugin = plugin;
	record.repository = repository->id;
	record.version = pluginManifest.version;
	record.revision = repository->revision;
	record.source = entry.source;

	for (int i = 0; i < s_installed.size(); i++)
	{
		if (s_installed[i].plugin == plugin)
		{
			s_installed.removeAt(i);
			break;
		}
	}

	s_installed << record;
	save();

	return true;
}

bool PluginRepository::uninstall(const QString& plugin, QString* error)
{
	QMutexLocker locker(&s_mutex);

	load();

	return uninstallLocked(plugin, error);
}

bool PluginRepository::uninstallLocked(const QString& plugin, QString* error)
{

	QString target = QDir(installedPath()).filePath(plugin);
	QFileInfo info(target);

	if (info.isSymLink() || info.isFile())
		QFile::remove(target);
	else if (info.isDir() && !QDir(target).removeRecursively())
	{
		if (error)
			*error = QObject::tr("failed to remove %1").arg(target);
		return false;
	}

	for (int i = 0; i < s_installed.size(); i++)
	{
		if (s_installed[i].plugin == plugin)
		{
			s_installed.removeAt(i);
			break;
		}
	}

	save();

	return true;
}

QList<PluginRepository::Available> PluginRepository::updates()
{
	QMutexLocker locker(&s_mutex);

	QList<Available> rv;

	foreach (const Available& entry, available())
	{
		if (entry.installedVersion.isEmpty() || entry.plugin.version.isEmpty())
			continue;
		if (entry.installedVersion != entry.plugin.version)
			rv << entry;
	}

	return rv;
}
