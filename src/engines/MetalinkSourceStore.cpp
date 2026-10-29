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

#include "MetalinkSourceStore.h"
#include "RuntimeException.h"
#include "config.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

MetalinkSourceStore::MetalinkSourceStore(const QString& dir)
	: m_dir(QDir::cleanPath(dir))
{
}

QString MetalinkSourceStore::defaultDir()
{
	return QDir::homePath() + QLatin1String(USER_PROFILE_PATH) + "/metalinks";
}

QString MetalinkSourceStore::keep(const QString& file, const QString& uuid) const
{
	QString id = uuid;
	id.remove('{').remove('}');

	const QString name = QFileInfo(file).fileName();
	if (id.isEmpty() || id.contains('/') || name.isEmpty())
		throw RuntimeException(QCoreApplication::translate("MetalinkDownload", "Cannot copy the metalink file %1").arg(file));

	const QString folder = m_dir + '/' + id;
	const QString copy = folder + '/' + name;

	QDir().mkpath(folder);
	QFile::remove(copy);
	if (!QFile::copy(file, copy))
		throw RuntimeException(QCoreApplication::translate("MetalinkDownload", "Cannot copy the metalink file %1").arg(file));
	return copy;
}

bool MetalinkSourceStore::owns(const QString& path) const
{
	return QDir::cleanPath(path).startsWith(m_dir + '/');
}

void MetalinkSourceStore::release(const QString& path) const
{
	if (!owns(path))
		return;

	// Only a direct child of the store may be removed, never the store itself.
	const QFileInfo folder(QFileInfo(QDir::cleanPath(path)).path());
	if (folder.path() == m_dir)
		QDir(folder.absoluteFilePath()).removeRecursively();
}

void MetalinkSourceStore::prune(const QSet<QString>& inUse) const
{
	QSet<QString> keepFolders;
	for (const QString& p : inUse)
		keepFolders << QFileInfo(QDir::cleanPath(p)).path();

	const QFileInfoList folders = QDir(m_dir).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
	for (const QFileInfo& f : folders)
	{
		if (!keepFolders.contains(f.absoluteFilePath()))
			QDir(f.absoluteFilePath()).removeRecursively();
	}
}
