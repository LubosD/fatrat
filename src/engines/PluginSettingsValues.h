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

#ifndef PLUGINSETTINGSVALUES_H
#define PLUGINSETTINGSVALUES_H
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>
#include <QVariant>
#include <functional>

/* How a Python plugin's `config` maps to stored settings, apart from any
 * widgets and from the settings store itself, so it can be tested with fakes.
 *
 * Settings pages know text, password, checkbox, combo, group and order only:
 * the web UI and the TUI render those and nothing else.
 */
namespace PluginSettingsValues
{
	typedef std::function<QVariant(const QString&, const QVariant&)> ValueGetter;
	typedef std::function<QList<QMap<QString, QVariant> >(const QString&)> ArrayGetter;
	typedef std::function<void(const QString&, const QVariant&)> ValueSetter;
	typedef std::function<void(const QString&, const QList<QMap<QString, QVariant> >&)> ArraySetter;

	// `config` without the items a settings page cannot show, inside groups
	// too. Each skipped type is logged once.
	QJsonArray settingsItems(const QJsonArray& config);

	// The stored values of every keyed item of `items` (as filtered by
	// settingsItems): a string array for order, a bool for checkbox and
	// group, a string for the rest.
	QJsonObject load(const QJsonArray& items, ValueGetter value, ArrayGetter array);

	// The opposite. Dispatches on the item's type, and writes only the keys
	// that `values` has.
	void save(const QJsonArray& items, const QJsonObject& values, ValueSetter setValue, ArraySetter setArray);
}

#endif
