/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2026 Lubos Dolezel <lubos a dolezel.info>

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

#include "WatchFolderService.h"
#include "WatchFile.h"
#include "Logger.h"
#include "RuntimeException.h"
#include <QFileInfo>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QScopeGuard>
#include <exception>

WatchFolderService* WatchFolderService::m_instance = nullptr;
// Guards m_instance against requestReload() racing with shutdown.
static QMutex s_instanceLock;

WatchFolderService::WatchFolderService(const Hooks& hooks)
	: m_hooks(hooks), m_scanner(hooks.clock)
{
	{
		QMutexLocker l(&s_instanceLock);
		m_instance = this;
	}

	connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &WatchFolderService::scan);
	connect(&m_rescan, &QTimer::timeout, this, [this] {
		reload();
		scanAll();
	});
	m_rescan.start(RescanMs);

	reload();
	QMetaObject::invokeMethod(this, &WatchFolderService::scanAll, Qt::QueuedConnection);
}

WatchFolderService::~WatchFolderService()
{
	QMutexLocker l(&s_instanceLock);
	m_instance = nullptr;
}

void WatchFolderService::requestReload()
{
	// Held across the post: ~QObject removes posted events, so the target cannot dangle.
	QMutexLocker l(&s_instanceLock);
	if (WatchFolderService* s = m_instance)
		QMetaObject::invokeMethod(s, &WatchFolderService::reload, Qt::QueuedConnection);
}

void WatchFolderService::reload()
{
	const QList<WatchFolderStatus::Entry> entries = m_hooks.folders();
	QHash<QString, QString> folders, statuses;

	for (const WatchFolderStatus::Entry& e : entries)
	{
		const QString dir = WatchFolderStatus::normalize(e.dir);
		const QString status = WatchFolderStatus::status(entries, e.queueUuid);
		statuses.insert(e.queueUuid, status);

		if (status != m_statuses.value(e.queueUuid, "off"))
		{
			if (status == "ok")
				log(tr("Watching %1").arg(dir));
			else if (status != "off")
				log(tr("%1: %2").arg(dir, WatchFolderStatus::describe(status)));
		}
		if (status == "ok")
			folders.insert(dir, e.queueUuid);
	}

	for (const QString& dir : m_watcher.directories())
	{
		if (!folders.contains(dir))
		{
			m_watcher.removePath(dir);
			m_scanner.forget(dir);
		}
	}
	for (auto it = folders.cbegin(); it != folders.cend(); ++it)
	{
		if (!m_watcher.directories().contains(it.key()))
		{
			m_watcher.addPath(it.key());
			scheduleFollowUp(it.key()); // a folder that just appeared may already hold files
		}
	}

	m_folders = folders;
	m_statuses = statuses;
}

void WatchFolderService::scanAll()
{
	const QStringList dirs = m_folders.keys();
	for (const QString& dir : dirs)
		scan(dir);
}

void WatchFolderService::scan(const QString& dir)
{
	const QString queueUuid = m_folders.value(dir);
	if (queueUuid.isEmpty())
		return;

	// The queue may have been deleted or the folder reassigned since the last reload; acting on stale
	// state would rename files of a queue that no longer exists to .failed.
	{
		const QList<WatchFolderStatus::Entry> entries = m_hooks.folders();
		bool valid = false;
		for (const WatchFolderStatus::Entry& e : entries)
		{
			if (e.queueUuid == queueUuid && WatchFolderStatus::normalize(e.dir) == dir
					&& WatchFolderStatus::status(entries, queueUuid) == "ok")
			{
				valid = true;
				break;
			}
		}
		if (!valid)
		{
			reload();
			return;
		}
	}

	const WatchFolderScanner::Result r = m_scanner.scan(dir);
	for (const QString& path : r.ready)
		process(path, queueUuid);
	if (r.pending)
		scheduleFollowUp(dir);
}

void WatchFolderService::scheduleFollowUp(const QString& dir)
{
	if (m_followUps.contains(dir))
		return;
	m_followUps.insert(dir);
	QTimer::singleShot(WatchFolderScanner::SettleMs + 100, this, [this, dir] {
		m_followUps.remove(dir);
		scan(dir);
	});
}

void WatchFolderService::process(const QString& path, const QString& queueUuid)
{
	// hooks.add may run a nested event loop in the future, during which another scan would still list
	// the not-yet-renamed file.
	if (m_inProgress.contains(path))
		return;
	m_inProgress.insert(path);
	const auto done = qScopeGuard([&] { m_inProgress.remove(path); });

	const QFileInfo info(path);
	const QString key = QString("%1|%2|%3").arg(path).arg(info.size()).arg(info.lastModified().toMSecsSinceEpoch());
	if (m_stuck.contains(key))
		return;

	const WatchFile::Contents contents = WatchFile::read(path);
	QString error = contents.error;
	int added = 0;

	if (error.isEmpty())
	{
		QStringList rejected = contents.skipped;
		try
		{
			added = m_hooks.add(queueUuid, contents.sources, rejected);
		}
		catch (const RuntimeException& e)
		{
			error = e.what();
		}
		catch (const std::exception& e)
		{
			error = QString::fromLocal8Bit(e.what());
		}

		const int MaxLogged = 10;
		for (int i = 0; i < rejected.size() && i < MaxLogged; i++)
			log(tr("%1: cannot add \"%2\"").arg(info.fileName(), rejected[i]));
		if (rejected.size() > MaxLogged)
			log(tr("%1: %2 more links were not accepted").arg(info.fileName()).arg(rejected.size() - MaxLogged));
		if (error.isEmpty() && !added)
			error = tr("nothing in it could be added");
	}

	if (added)
		log(tr("%1: added %2 transfer(s)").arg(info.fileName()).arg(added));
	else
		log(tr("%1: %2").arg(info.fileName(), error));

	// A file replaced while it was being added is a different file; leave it for the next round. Only the size
	// is compared: some network filesystems report a slightly different mtime on every stat, and skipping the
	// rename because of that would add the same file again on every scan.
	const QFileInfo now(path);
	if (now.exists() && now.size() != info.size())
	{
		log(tr("%1: the file changed while being added; it will be processed again").arg(info.fileName()));
		return;
	}

	if (WatchFile::markProcessed(path, added > 0).isEmpty())
	{
		log(tr("%1: cannot rename the file; it will not be picked up again until FatRat restarts").arg(info.fileName()));
		m_stuck.insert(key);
	}
}

void WatchFolderService::log(const QString& msg)
{
	Logger::global()->enterLogMessage("Watch folder", msg);
}
