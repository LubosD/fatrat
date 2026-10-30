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

#ifndef METALINKSOURCESTORE_H
#define METALINKSOURCESTORE_H
#include <QSet>
#include <QString>

// Private copies of local .metalink/.meta4 files. MetalinkDownload reads its source only when it starts,
// and by then a watch folder has renamed the original and a REST upload's temporary file is gone.
// Each copy is <dir>/<transfer uuid>/<original name>, so the transfer keeps showing the original name.
class MetalinkSourceStore
{
public:
	explicit MetalinkSourceStore(const QString& dir = defaultDir());
	static QString defaultDir(); // ~/.local/share/fatrat/metalinks

	// Returns the copy's path; throws RuntimeException if the file cannot be copied.
	QString keep(const QString& file, const QString& uuid) const;
	bool owns(const QString& path) const;
	void release(const QString& path) const; // deletes path and its folder if it is one of the copies
	void prune(const QSet<QString>& inUse) const; // deletes the copies not in inUse
private:
	QString m_dir;
};

#endif
