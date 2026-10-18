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

#ifndef TRANSFERACTIONS_H
#define TRANSFERACTIONS_H
#include <QList>
#include <optional>

#include "TransferAction.h"

// A plugin threw out of its code; already logged when this is thrown.
struct PluginError
{
	QString plugin;
	QString message;
};

/* The registry behind registerTransferAction(), used by the front ends. */
namespace TransferActions
{
	struct Entry
	{
		TransferAction action;
		QString plugin; // file name of the plugin that registered it, "fatrat" for built-ins
	};

	// An appliesTo that throws counts as false (and is logged).
	bool applies(const Entry& entry, Transfer* t);
	QList<Entry> applicable(Transfer* t);
	std::optional<Entry> find(const QString& id);

	// Runs the trigger and returns the task it created, if any. Throws PluginError
	// if the plugin threw; a task it had created by then is marked failed.
	Task* trigger(const Entry& entry, Transfer* t, const QString& transferUuid, const QString& transferName);

	// What registrations are attributed to while the loader runs a plugin's initPlugin().
	void setLoadingPlugin(const QString& name);
	// Removes everything a plugin registered; for a plugin whose initPlugin() threw.
	void removePlugin(const QString& name);
	void clear(); // for the tests
}

#endif
