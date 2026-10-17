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

#ifndef FORMVALIDATOR_H
#define FORMVALIDATOR_H
#include <QJsonObject>
#include <QString>

/* The forms a plugin shows through Task::ask(), checked in both directions.
 *
 * A form is data so that every front end can render it. That also means a
 * plugin can hand over nonsense, and a client can answer with nonsense; both
 * are caught here, before either reaches the other side.
 *
 * The vocabulary is the Python plugins' `config` one (text, password,
 * checkbox, combo, group) plus label, directory, list and checklist.
 */
namespace FormValidator
{
	// An empty string if `form` is valid, otherwise what is wrong with it.
	QString checkForm(const QJsonObject& form);

	// An empty string if `answer` is a valid answer to `form`, otherwise what
	// is wrong with it. On success `normalized` gets exactly one value per
	// keyed field; anything else the client sent is dropped.
	QString checkAnswer(const QJsonObject& form, const QJsonObject& answer, QJsonObject* normalized);
}

#endif
