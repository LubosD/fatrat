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

#ifndef PLUGINAPI_H
#define PLUGINAPI_H

/* What a native plugin uses for transfer actions and tasks, in every FatRat
 * build, desktop or headless.
 *
 * Besides getInfo(), such a plugin exports:
 *
 *   FATRAT_PLUGIN_EXPORT int getApiVersion() { return FATRAT_PLUGIN_API_VERSION; }
 *   FATRAT_PLUGIN_EXPORT bool requiresGui() { return false; }
 *   FATRAT_PLUGIN_EXPORT void initPlugin() { registerTransferAction(...); }
 *
 * and registers nothing from static constructors: those run inside dlopen(),
 * before FatRat has decided to keep the plugin, and a plugin it then unloads
 * would leave its registrations pointing at unloaded code.
 *
 * A plugin with requiresGui() returning false must not use QtWidgets or the
 * desktop-only functions of fatrat.h; it then works the same everywhere.
 *
 * Build a plugin against the headers of the FatRat it is loaded into: the
 * layout of Transfer differs between desktop and headless builds (virtual
 * functions after its GUI-only ones move), so a plugin built for one can call
 * the wrong function in the other.
 *
 * A plugin that runs threads of its own must stop and join them when
 * QCoreApplication::aboutToQuit fires; FatRat cancels running tasks first, so
 * a worker that checks Task::cancelRequested() ends promptly.
 */
#include "Task.h"
#include "TransferAction.h"

#define FATRAT_PLUGIN_API_VERSION 1
#define FATRAT_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))

#endif
