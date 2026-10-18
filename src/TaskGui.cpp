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

#include "TaskGui.h"
#include "FormDialog.h"
#include "TaskStatusWidget.h"
#include "fatrat.h"

#include "plugins/Task.h"
#include "plugins/TaskManager.h"

TaskGui::TaskGui(QObject* parent)
	: QObject(parent)
{
	TaskManager* manager = TaskManager::instance();

	connect(manager, &TaskManager::taskAdded, this, &TaskGui::taskAdded);
	connect(manager, &TaskManager::promptOpened, this, &TaskGui::showPrompt);

	for (Task* task : manager->tasks())
	{
		taskAdded(task);
		if (task->prompt())
			showPrompt(task);
	}
}

void TaskGui::taskAdded(Task* task)
{
	addStatusWidget(new TaskStatusWidget(task, this), false);
}

void TaskGui::showPrompt(Task* task)
{
	std::optional<Task::Prompt> prompt = task->prompt();

	if (!prompt)
		return;

	QPointer<FormDialog>& dlg = m_dialogs[prompt->id];
	if (!dlg)
	{
		FormDialog* created = new FormDialog(task);

		// The question may have closed in between; the dialog deletes itself then.
		if (!created->isOpen())
		{
			m_dialogs.remove(prompt->id);
			return;
		}
		dlg = created;
		connect(created, &QObject::destroyed, this, [this, id = prompt->id] { m_dialogs.remove(id); });
	}
	dlg->show();
	dlg->raise();
	dlg->activateWindow();
}
