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

#ifndef FORMWIDGETS_H
#define FORMWIDGETS_H
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QPair>
#include <QString>

class QWidget;

/* The form vocabulary (see plugins/FormValidator.h) as Qt widgets, shared by
 * plugin questions (FormDialog) and Python plugins' settings pages.
 *
 * Values are JSON: a string for text, password, directory, combo and list; a
 * bool for checkbox and a keyed group; an array of strings for checklist and
 * order. Each keyed widget gets its key as object name.
 *
 * The widgets are owned by the parent passed to the constructor; this object
 * only points into them, so it must not outlive that parent.
 */
class FormWidgets
{
public:
	// Lays `items` out in `parent`, which must not have a layout yet. A keyed
	// field starts with values[key] if given, else with its "default" (a
	// checklist with its rows' "checked", a list with its first row).
	FormWidgets(const QJsonArray& items, const QJsonObject& values, QWidget* parent);

	QJsonObject values() const;

	// (key, type) of every keyed item, inside groups too.
	static QList<QPair<QString, QString>> keys(const QJsonArray& items);
private:
	struct Bound
	{
		QString key, type;
		QWidget* widget;
	};

	void addItems(const QJsonArray& items, QWidget* parent);
	void bind(const QString& key, const QString& type, QWidget* widget);
	QJsonValue initial(const QJsonObject& item) const;

	QJsonObject m_values;
	QList<Bound> m_bound;
};

#endif
