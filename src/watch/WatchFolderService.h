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

#ifndef WATCHFOLDERSERVICE_H
#define WATCHFOLDERSERVICE_H
#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <functional>
#include "WatchFolderScanner.h"
#include "WatchFolderStatus.h"

// Adds the files dropped into queues' watch folders. Runs on the main thread, with or without the GUI.
// Native change notifications (QFileSystemWatcher) give a quick reaction on local disks; the periodic
// rescan catches what they miss, such as writes from other hosts to a network mount.
class WatchFolderService : public QObject
{
Q_OBJECT
public:
	struct Hooks
	{
		// The queues' watch folders, in queue order.
		std::function<QList<WatchFolderStatus::Entry>()> folders;
		// Adds the sources to the queue and returns how many transfers were created. Sources no transfer
		// class accepts go to rejected. Throws RuntimeException when nothing could be added at all.
		std::function<int(const QString& queueUuid, const QStringList& sources, QStringList& rejected)> add;
		WatchFolderScanner::Clock clock; // optional
	};
	static constexpr int RescanMs = 60000;

	explicit WatchFolderService(const Hooks& hooks);
	~WatchFolderService();
	static WatchFolderService* instance() { return m_instance; }

	// Callable from any thread: rereads the configuration on the main thread.
	static void requestReload();
public slots:
	void reload();
	void scanAll();
	void scan(const QString& dir);
private:
	void process(const QString& path, const QString& queueUuid);
	void scheduleFollowUp(const QString& dir);
	static void log(const QString& msg);

	Hooks m_hooks;
	WatchFolderScanner m_scanner;
	QFileSystemWatcher m_watcher;
	QTimer m_rescan;
	QHash<QString, QString> m_folders;  // normalized folder -> queue UUID, only folders with status "ok"
	QHash<QString, QString> m_statuses; // queue UUID -> last logged status
	QSet<QString> m_followUps;          // folders with a follow-up scan scheduled
	QSet<QString> m_stuck;              // processed files that could not be renamed
	static WatchFolderService* m_instance;
};

#endif
