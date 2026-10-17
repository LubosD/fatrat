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

#include "PluginLease.h"

#include <QCoreApplication>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>

#include "PluginRegistry.h"

static QMutex s_mutex;

static QHash<QString, int>& counts()
{
	static QHash<QString, int> counts;
	return counts;
}

PluginLease::PluginLease(const QString& identifier)
	: m_identifier(identifier)
{
	QMutexLocker locker(&s_mutex);
	counts()[m_identifier]++;
}

PluginLease::~PluginLease()
{
	bool idle;

	{
		QMutexLocker locker(&s_mutex);
		int& n = counts()[m_identifier];

		idle = (--n <= 0);
		if (idle)
			counts().remove(m_identifier);
	}

	// Only matters if a plugin is waiting for its transfers to end, and is
	// cheap when none is.
	if (idle && QCoreApplication::instance())
	{
		QMetaObject::invokeMethod(QCoreApplication::instance(),
			[] { PluginRegistry::finishDrains(); }, Qt::QueuedConnection);
	}
}

int PluginLease::count(const QString& identifier)
{
	QMutexLocker locker(&s_mutex);
	return counts().value(identifier);
}
