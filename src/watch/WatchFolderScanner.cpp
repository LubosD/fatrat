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

#include "WatchFolderScanner.h"
#include "WatchFile.h"
#include <QDir>
#include <chrono>

WatchFolderScanner::WatchFolderScanner(Clock clock)
	: m_clock(clock)
{
	if (!m_clock)
	{
		m_clock = [] {
			return qint64(std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now().time_since_epoch()).count());
		};
	}
}

WatchFolderScanner::Result WatchFolderScanner::scan(const QString& dir)
{
	const qint64 now = m_clock();
	const QHash<QString, Seen> before = m_seen.value(dir);
	QHash<QString, Seen> after;
	Result r;

	// QDir::Files leaves out hidden files and directories
	const QFileInfoList entries = QDir(dir).entryInfoList(QDir::Files, QDir::Name);
	for (const QFileInfo& info : entries)
	{
		if (WatchFile::kind(info.fileName()) == WatchFile::Unsupported)
			continue;

		Seen s{ info.size(), info.lastModified(), now };
		auto it = before.constFind(info.fileName());
		if (it != before.cend() && it->size == s.size && it->mtime == s.mtime)
			s.since = it->since;
		after.insert(info.fileName(), s);

		if (now - s.since >= SettleMs)
			r.ready << info.absoluteFilePath();
		else
			r.pending = true;
	}

	m_seen.insert(dir, after);
	return r;
}

void WatchFolderScanner::forget(const QString& dir)
{
	m_seen.remove(dir);
}
