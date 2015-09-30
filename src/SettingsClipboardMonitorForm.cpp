/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2010 Lubos Dolezel <lubos a dolezel.info>

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

#include "SettingsClipboardMonitorForm.h"
#include "Settings.h"
#include "ClipboardMonitor.h"

SettingsClipboardMonitorForm::SettingsClipboardMonitorForm(QWidget* me, QObject* parent) :
    QObject(parent)
{
	setupUi(me);
	connect(pushAdd, SIGNAL(clicked()), this, SLOT(actionAdd()));
	connect(pushEdit, SIGNAL(clicked()), this, SLOT(actionEdit()));
	connect(pushDelete, SIGNAL(clicked()), this, SLOT(actionDelete()));
}

void SettingsClipboardMonitorForm::load()
{
	listRegexps->clear();

	checkEnableGlobal->setChecked(getSettingsValue("clipboard/monitorglobal").toBool());
	checkEnableSelection->setChecked(getSettingsValue("clipboard/monitorselection").toBool());
	QStringList regexps = getSettingsValue("clipboard/regexps").toStringList();
	foreach (QString regexp, regexps)
	{
		QListWidgetItem* item = new QListWidgetItem(QIcon(":/fatrat/miscellaneous.png"), regexp, listRegexps);
		listRegexps->addItem(item);
		item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
	}
}

void SettingsClipboardMonitorForm::accepted()
{
	setSettingsValue("clipboard/monitorglobal", checkEnableGlobal->isChecked());
	setSettingsValue("clipboard/monitorselection", checkEnableSelection->isChecked());

	QStringList regexps;
	for (int i=0;i<listRegexps->count();i++)
	{
		QListWidgetItem* item = listRegexps->item(i);
		regexps << item->text();
	}

	setSettingsValue("clipboard/regexps", regexps);
	applySettings();
}

void SettingsClipboardMonitorForm::applySettings()
{
	ClipboardMonitor::instance()->reloadSettings();
}

void SettingsClipboardMonitorForm::actionAdd()
{
	QListWidgetItem* item = new QListWidgetItem(QIcon(":/fatrat/miscellaneous.png"), QString(), listRegexps);
	listRegexps->addItem(item);
	item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
	listRegexps->editItem(item);
}

void SettingsClipboardMonitorForm::actionEdit()
{
	if(QListWidgetItem* item = listRegexps->currentItem())
		listRegexps->editItem(item);
}

void SettingsClipboardMonitorForm::actionDelete()
{
	int i = listRegexps->currentRow();
	if(i != -1)
		delete listRegexps->takeItem(i);
}
