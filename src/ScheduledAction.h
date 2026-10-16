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

#ifndef SCHEDULEDACTION_H
#define SCHEDULEDACTION_H
#include <QUuid>
#include <QString>
#include <QDateTime>
#include <QTime>
#include <QList>
#include <QVariant>
#include <cstring>

struct ScheduledAction
{
	ScheduledAction()
	{
		memset(daysRepeated, 1, sizeof(daysRepeated));
		repeated = true;
		action = ActionResumeAll;
		whenRepeated = QTime::currentTime();
		whenOneTime = QDateTime::currentDateTime();
	}
	
	enum ActionType
	{
		ActionResumeAll, ActionStopAll, ActionSetSpeedLimit
	};
	
	QString id;
	QString name;
	QUuid queue;
	ActionType action;
	QVariant actionArgument;
	QDateTime whenOneTime;
	QTime whenRepeated;
	bool daysRepeated[7];
	bool repeated;

	static QString newId();
	// Entries saved before ids existed get one, and the list is saved again
	static void loadAll(QList<ScheduledAction>& list);
	static void saveAll(const QList<ScheduledAction>& list);
};

#endif
