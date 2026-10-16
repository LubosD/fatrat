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

#include "DirectoryListing.h"
#include "RestApi.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>

QJsonObject DirectoryListing::list(const QString& path, const QString& fallback, bool showHidden)
{
	const QString requested = path.isEmpty() ? fallback : path;

	if (!QDir::isAbsolutePath(requested))
		throw RestError(422, "invalid_path", "path must be absolute");

	const QString canonical = QFileInfo(requested).canonicalFilePath();
	if (canonical.isEmpty() || !QFileInfo(canonical).isDir())
		throw RestError(404, "path_not_found", "No such directory");

	QDir dir(canonical);
	if (!QFileInfo(canonical).isReadable() || !QFileInfo(canonical).isExecutable())
		throw RestError(403, "path_not_readable", "Permission denied");

	QDir::Filters filters = QDir::Dirs | QDir::NoDotAndDotDot;
	if (showHidden)
		filters |= QDir::Hidden;

	QJsonArray entries;
	for (const QFileInfo& info : dir.entryInfoList(filters, QDir::Name | QDir::IgnoreCase))
	{
		QJsonObject e;
		e["name"] = info.fileName();
		e["path"] = info.absoluteFilePath();
		entries << e;
	}

	QDir parent(canonical);
	QJsonObject rv;
	rv["path"] = canonical;
	rv["parent"] = parent.cdUp() ? parent.absolutePath() : QString();
	rv["entries"] = entries;
	return rv;
}
