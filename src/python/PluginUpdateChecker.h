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

#ifndef PLUGINUPDATECHECKER_H
#define PLUGINUPDATECHECKER_H

#include "config.h"
#ifndef WITH_PYPLUGINS
#	error This file is not supposed to be included!
#endif

#include <QObject>
#include <QTimer>

/* Optional, off by default: fetch the user's plugin repositories now and
 * then and say how many installed plugins have a newer version.
 *
 * Fetching is git over the network, so it happens on a worker thread; only
 * the result comes back to the main one. Nothing is installed automatically
 * -- an update is always the user's decision.
 */
class PluginUpdateChecker : public QObject
{
Q_OBJECT
public:
	PluginUpdateChecker(QObject* parent = 0);

	// Honours python/check_updates; call again when the setting changes.
	void applySettings();
public slots:
	// Refresh in the background and report. Safe to call while one is
	// already running: it does nothing.
	void checkNow();
signals:
	void updatesAvailable(int count);
private slots:
	void finished(int count);
private:
	QTimer m_timer;
	bool m_bRunning;
};

#endif // PLUGINUPDATECHECKER_H
