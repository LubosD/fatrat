/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2009 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#include "SettingsSchedulerForm.h"
#include "Settings.h"
#include "ScheduledActionDlg.h"
#include <QMessageBox>

SettingsSchedulerForm::SettingsSchedulerForm(QWidget* w, QObject* parent) : QObject(parent)
{
	setupUi(w);
	
	connect(pushAdd, SIGNAL(clicked()), this, SLOT(add()));
	connect(pushEdit, SIGNAL(clicked()), this, SLOT(edit()));
	connect(pushDelete, SIGNAL(clicked()), this, SLOT(remove()));
}

void SettingsSchedulerForm::load()
{
	listTasks->clear();
	Scheduler::loadActions(m_actions);

	foreach(const ScheduledAction& a, m_actions)
		listTasks->addItem(a.name);
}

void SettingsSchedulerForm::accepted()
{
	Scheduler::saveActions(m_actions);
	applySettings();
}

void SettingsSchedulerForm::applySettings()
{
	Scheduler::instance()->reload();
}

void SettingsSchedulerForm::add()
{
	ScheduledActionDlg dlg(pushAdd->parentWidget());
	if (dlg.exec() != QDialog::Accepted)
		return;
	m_actions << dlg.m_action;
	listTasks->addItem(dlg.m_action.name);
}

void SettingsSchedulerForm::edit()
{
	int index = listTasks->currentRow();
	if(index < 0)
		return;

	ScheduledActionDlg dlg(pushAdd->parentWidget());
	dlg.m_action = m_actions[index];

	if (dlg.exec() != QDialog::Accepted)
		return;

	m_actions[index] = dlg.m_action;
	listTasks->item(index)->setText(dlg.m_action.name);
}

void SettingsSchedulerForm::remove()
{
	int index = listTasks->currentRow();
	if(index < 0)
		return;
	if(QMessageBox::warning(pushAdd->parentWidget(), "FatRat", tr("Do you really want to remove the selected scheduled action?"),
				QMessageBox::Yes|QMessageBox::No) != QMessageBox::Yes)
	{
		return;
	}

	delete listTasks->takeItem(index);
	m_actions.removeAt(index);
}
