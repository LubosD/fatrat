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

#ifndef TASKGUI_H
#define TASKGUI_H
#include <QHash>
#include <QObject>
#include <QPointer>

class FormDialog;
class Task;

/* Plugin tasks on the desktop: a status-bar widget per task, and a dialog for
 * every question, shown as soon as it is asked (as CaptchaQt does). */
class TaskGui : public QObject
{
Q_OBJECT
public:
	explicit TaskGui(QObject* parent);

	// Shows the task's open question, or brings its dialog to the front.
	void showPrompt(Task* task);
private:
	void taskAdded(Task* task);

	QHash<QString, QPointer<FormDialog>> m_dialogs; // by prompt id
};

#endif
