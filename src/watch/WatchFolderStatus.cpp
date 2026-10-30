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
#include "WatchFolderStatus.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace WatchFolderStatus
{

QString normalize(const QString& dir)
{
	if (dir.isEmpty())
		return QString();
	return QDir::cleanPath(QFileInfo(dir).absoluteFilePath());
}

QString owner(const QList<Entry>& entries, const QString& dir, const QString& exceptUuid)
{
	const QString wanted = normalize(dir);
	if (wanted.isEmpty())
		return QString();

	for (const Entry& e : entries)
	{
		if (e.queueUuid != exceptUuid && normalize(e.dir) == wanted)
			return e.queueUuid;
	}
	return QString();
}

QString status(const QList<Entry>& entries, const QString& queueUuid)
{
	for (int i = 0; i < entries.size(); i++)
	{
		if (entries[i].queueUuid != queueUuid)
			continue;

		const QString dir = normalize(entries[i].dir);
		if (dir.isEmpty())
			return "off";

		for (int j = 0; j < i; j++)
		{
			if (normalize(entries[j].dir) == dir)
				return "duplicate";
		}

		const QFileInfo info(dir);
		if (!info.isDir())
			return "missing";
		if (!info.isReadable() || !info.isWritable() || !info.isExecutable()) // x: to create and rename entries
			return "not_writable";
		return "ok";
	}
	return "off";
}

QString describe(const QString& status)
{
	if (status == "missing")
		return QCoreApplication::translate("WatchFolderStatus", "The folder doesn't exist. FatRat keeps checking and starts watching it once it appears.");
	if (status == "not_writable")
		return QCoreApplication::translate("WatchFolderStatus", "FatRat can't write to this folder, so it can't mark files as processed. Nothing is picked up.");
	if (status == "duplicate")
		return QCoreApplication::translate("WatchFolderStatus", "Another queue already watches this folder, so this setting is ignored.");
	return QString();
}

}
