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
#ifndef WATCHFOLDERSTATUS_H
#define WATCHFOLDERSTATUS_H
#include <QList>
#include <QString>

// Whether a queue's watch folder works. Shared by the watcher, the REST API and the desktop dialog.
namespace WatchFolderStatus
{
	struct Entry
	{
		QString queueUuid;
		QString dir; // as configured; empty = no watch folder
	};

	QString normalize(const QString& dir); // absolute and cleaned; empty stays empty

	// The UUID of the first queue other than exceptUuid that watches dir, or an empty string.
	QString owner(const QList<Entry>& entries, const QString& dir, const QString& exceptUuid = QString());

	// "off", "ok", "missing", "not_writable" or "duplicate". entries are in queue order:
	// of two queues watching one folder, the first one wins.
	QString status(const QList<Entry>& entries, const QString& queueUuid);

	// What is wrong, for the user; empty for "off" and "ok".
	QString describe(const QString& status);
}

#endif
