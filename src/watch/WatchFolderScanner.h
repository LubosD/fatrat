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

#ifndef WATCHFOLDERSCANNER_H
#define WATCHFOLDERSCANNER_H
#include <QDateTime>
#include <QHash>
#include <QStringList>
#include <functional>

// Lists watch folders and reports the files that stopped changing. A file is ready once its size and
// modification time stayed the same for SettleMs, so a file still being copied in is not read half-way.
class WatchFolderScanner
{
public:
	using Clock = std::function<qint64()>; // milliseconds, monotonic
	static const qint64 SettleMs = 2000;

	struct Result
	{
		QStringList ready; // absolute paths, reported on every scan until the file is gone
		bool pending = false; // some files are still settling; scan again after SettleMs
	};

	explicit WatchFolderScanner(Clock clock = Clock());

	Result scan(const QString& dir); // not recursive
	void forget(const QString& dir); // drops what was observed in a folder that is no longer watched
private:
	struct Seen
	{
		qint64 size;
		QDateTime mtime;
		qint64 since;
	};

	Clock m_clock;
	QHash<QString, QHash<QString, Seen>> m_seen; // folder -> file name -> observation
};

#endif
