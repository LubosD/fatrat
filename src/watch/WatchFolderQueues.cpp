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

#include "WatchFolderQueues.h"
#include "Queue.h"
#include "TransferAdder.h"
#include <QCoreApplication>
#include <QReadLocker>

namespace WatchFolderQueues
{

QList<WatchFolderStatus::Entry> entries()
{
	QList<WatchFolderStatus::Entry> rv;
	for (Queue* q : g_queues)
		rv << WatchFolderStatus::Entry{ q->uuid(), q->watchDirectory() };
	return rv;
}

WatchFolderService::Hooks hooks()
{
	WatchFolderService::Hooks h;

	h.folders = [] {
		QReadLocker l(&g_queuesLock);
		return entries();
	};

	h.add = [](const QString& queueUuid, const QStringList& sources, QStringList& rejected) -> int {
		int added = -1;
		{
			QReadLocker l(&g_queuesLock);
			for (Queue* q : g_queues)
			{
				if (q->uuid() == queueUuid)
				{
					added = TransferAdder::add(q, sources, TransferAdder::Options(), &rejected).size();
					break;
				}
			}
		}
		if (added < 0)
			throw RuntimeException(QCoreApplication::translate("WatchFolderQueues", "The queue no longer exists"));
		if (added > 0)
			Queue::saveQueuesAsync();
		return added;
	};

	return h;
}

}
