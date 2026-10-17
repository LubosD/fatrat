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

#ifndef PLUGINLEASE_H
#define PLUGINLEASE_H

#include "config.h"
#ifndef WITH_PYPLUGINS
#	error This file is not supposed to be included!
#endif

#include <QString>

/* A transfer's claim on its plugin.
 *
 * A plugin that was disabled or uninstalled is not unloaded while transfers
 * created from it still exist: they hold instances of its classes, and a
 * paused one can be resumed. Each such transfer owns one of these, keyed by
 * the engine identifier; when the last one goes away the registry is told
 * (on the main thread, never from inside the transfer's destructor) and
 * finishes the unload.
 */
class PluginLease
{
public:
	explicit PluginLease(const QString& identifier);
	~PluginLease();

	// Transfers currently alive for this engine identifier.
	static int count(const QString& identifier);

private:
	PluginLease(const PluginLease&);
	PluginLease& operator=(const PluginLease&);

	QString m_identifier;
};

#endif // PLUGINLEASE_H
