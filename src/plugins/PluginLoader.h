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

#ifndef PLUGINLOADER_H
#define PLUGINLOADER_H
#include <QString>
#include <optional>

#include "fatrat.h"

/* Loads one native plugin and decides whether it may stay.
 *
 * Plugins written for the plugin API (PluginApi.h) say which API version they
 * were built for and whether they need the desktop interface, and register
 * their actions from initPlugin(), which runs only once they are accepted.
 * Older plugins register from static constructors, which have already run by
 * the time FatRat can look at them, so those are never unloaded again.
 */
namespace PluginLoader
{
	enum Outcome { Loaded, LoadedLegacy, SkippedNeedsGui, Incompatible, Failed };

	struct Result
	{
		Outcome outcome = Failed;
		QString message; // why it was not loaded
		// Only for loaded plugins: it points into the plugin, which stays mapped.
		std::optional<PluginInfo> info;
	};

	Result load(const QString& path, bool haveGui);
}

#endif
