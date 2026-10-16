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

#include "ScheduledAction.h"
#include <QSettings>

extern QSettings* g_settings;

QString ScheduledAction::newId()
{
	return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void ScheduledAction::loadAll(QList<ScheduledAction>& list)
{
	bool bMissingId = false;
	int num = g_settings->beginReadArray("scheduler/actions");
	
	list.clear();
	
	for(int i=0;i<num;i++)
	{
		ScheduledAction a;
		g_settings->setArrayIndex(i);
		a.id = g_settings->value("id").toString();
		if(a.id.isEmpty())
		{
			a.id = newId();
			bMissingId = true;
		}
		a.name = g_settings->value("name").toString();
		a.queue = QUuid(g_settings->value("queueUUID").toString());
		a.action = (ScheduledAction::ActionType) g_settings->value("action").toInt();
		a.whenOneTime = QDateTime::fromString(g_settings->value("whenOneTime").toString(), Qt::ISODate);
		a.whenRepeated = QTime::fromString(g_settings->value("whenRepeated").toString(), Qt::ISODate);
		a.repeated = g_settings->value("repeated").toBool();
		a.actionArgument = g_settings->value("actionArgument");
		
		QString days = g_settings->value("daysRepeated").toString();
		for(int i=0;i<7;i++)
			a.daysRepeated[i] = i < days.size() && days[i] == '1';
		list << a;
	}
	
	g_settings->endArray();

	if(bMissingId)
		saveAll(list);
}

void ScheduledAction::saveAll(const QList<ScheduledAction>& items)
{
	g_settings->remove("scheduler/actions");
	g_settings->beginWriteArray("scheduler/actions", items.size());
	
	for(int i=0;i<items.size();i++)
	{
		const ScheduledAction& a = items[i];
		
		g_settings->setArrayIndex(i);
		g_settings->setValue("id", a.id);
		g_settings->setValue("name", a.name);
		g_settings->setValue("queueUUID", a.queue.toString());
		g_settings->setValue("action", int(a.action));
		g_settings->setValue("whenOneTime", a.whenOneTime.toString(Qt::ISODate));
		g_settings->setValue("whenRepeated", a.whenRepeated.toString(Qt::ISODate));
		g_settings->setValue("repeated", a.repeated);
		g_settings->setValue("actionArgument", a.actionArgument);
		
		char days[8];
		for(int i=0;i<7;i++)
			days[i] = a.daysRepeated[i] ? '1' : '0';
		days[7] = 0;
		
		g_settings->setValue("daysRepeated", QString(days));
	}
	
	g_settings->endArray();
}
