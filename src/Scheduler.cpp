/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2009 Lubos Dolezel <lubos a dolezel.info>

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

#include "Scheduler.h"
#include "Settings.h"
#include "Queue.h"
#include "Logger.h"
#include <QReadLocker>

Scheduler* Scheduler::m_instance = 0;

Scheduler::Scheduler()
{
	reload();
	m_lastChecked = QDateTime::currentDateTime();

	connect(&m_timer, SIGNAL(timeout()), this, SLOT(doWork()));
	m_timer.start(10*1000);
	m_instance = this;
}

Scheduler::~Scheduler()
{
	m_instance = 0;
}

void Scheduler::doWork()
{
	const QDateTime now = QDateTime::currentDateTime();

	// Every minute is handled exactly once, however late or early the timer fires
	for(const QDateTime& minute : ScheduledAction::minutesToCheck(m_lastChecked, now))
	{
		for(ScheduledAction& a : m_actions)
		{
			if(!a.dueAt(minute))
				continue;
			if(!a.repeated)
			{
				a.done = true;
				ScheduledAction::markDone(a.id);
			}
			executeAction(a);
		}
	}
	m_lastChecked = now;
}

void Scheduler::executeAction(const ScheduledAction& action)
{
	QReadLocker l(&g_queuesLock);
	bool bFound = false;

	for(int i=0;i<g_queues.size();i++)
	{
		Queue* q = g_queues[i];
		if(q->uuid() == action.queue.toString())
		{
			Logger::global()->enterLogMessage(tr("Scheduler"), tr("Executing a scheduled action: %1").arg(action.name));

			switch(action.action)
			{
			case ScheduledAction::ActionResumeAll:
				q->resumeAll();
				break;
			case ScheduledAction::ActionStopAll:
				q->stopAll();
				break;
			case ScheduledAction::ActionSetSpeedLimit:
				{
					QList<QVariant> sp = action.actionArgument.toList();
					q->setSpeedLimits(sp[0].toInt(), sp[1].toInt());
					break;
				}
			}
			bFound = true;
			break;
		}
	}

	if(!bFound)
	{
		Logger::global()->enterLogMessage(tr("Scheduler"), tr("Failed to execute a scheduled action: %1, the queue doesn't seem to exist any more").arg(action.name));
	}
}

void Scheduler::reload()
{
	ScheduledAction::loadAll(m_actions);
}


void Scheduler::queueRemoved(const QString& uuid)
{
	if(ScheduledAction::removeForQueue(QUuid(uuid)) && m_instance)
		m_instance->reload();
}
