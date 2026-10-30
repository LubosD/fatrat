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

#ifndef WATCHFILE_H
#define WATCHFILE_H
#include <QString>
#include <QStringList>

// One file picked up from a watch folder: what it holds and how it is marked as processed.
namespace WatchFile
{
	enum Kind { Unsupported, Torrent, Metalink, LinkList };

	// By extension, case-insensitively. Hidden files are Unsupported; so are partial downloads
	// (foo.torrent.part) and processed files (foo.torrent.added), as their extension does not match.
	Kind kind(const QString& fileName);

	const qint64 MaxListSize = 1024 * 1024;

	struct Contents
	{
		QStringList sources; // the file itself, or the links of a link list
		QStringList skipped; // lines of a link list that are not links
		QString error;       // set when there is nothing to add: unsupported, unreadable, too large or empty
	};
	// A link is a line of the form scheme:something, without spaces; whether a transfer class accepts it is up
	// to the caller.
	Contents read(const QString& path);

	// Renames the file to <name>.added or <name>.failed, appending .1, .2, ... if that name is taken.
	// Returns the new path, or an empty string if the rename failed: the file is then still in the folder,
	// and the caller must remember it so it is not added again.
	QString markProcessed(const QString& path, bool added);
}

#endif
