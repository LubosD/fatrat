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

#include "WatchFile.h"
#include <QFile>
#include <QFileInfo>

namespace WatchFile
{

Kind kind(const QString& fileName)
{
	const QString name = fileName.toLower();

	if (name.startsWith('.'))
		return Unsupported;
	if (name.endsWith(".torrent"))
		return Torrent;
	if (name.endsWith(".metalink") || name.endsWith(".meta4"))
		return Metalink;
	if (name.endsWith(".txt") || name.endsWith(".magnet"))
		return LinkList;
	return Unsupported;
}

Contents read(const QString& path)
{
	Contents c;
	const QFileInfo info(path);

	if (kind(info.fileName()) != LinkList)
	{
		c.sources << info.absoluteFilePath();
		return c;
	}

	if (info.size() > MaxListSize)
	{
		c.error = QString("The link list is larger than %1 bytes").arg(MaxListSize);
		return c;
	}

	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
	{
		c.error = file.errorString();
		return c;
	}

	QString text = QString::fromUtf8(file.readAll());
	if (text.startsWith(QChar(0xFEFF)))
		text.remove(0, 1);

	for (const QString& raw : text.split('\n'))
	{
		const QString line = raw.trimmed();
		if (!line.isEmpty() && !line.startsWith('#'))
			c.sources << line;
	}

	if (c.sources.isEmpty())
		c.error = "The link list holds no links";
	return c;
}

QString markProcessed(const QString& path, bool added)
{
	const QString base = path + (added ? ".added" : ".failed");
	QString target = base;

	for (int i = 1; QFileInfo::exists(target); i++)
		target = base + '.' + QString::number(i);

	return QFile::rename(path, target) ? target : QString();
}

}
