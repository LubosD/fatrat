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

#ifndef SCHEDULER_H
#define SCHEDULER_H
#include <QUuid>
#include <QString>
#include <QThread>
#include <QDateTime>
#include <QTime>
#include <QTimer>
#include <QVariant>

struct ScheduledAction;

class Scheduler : public QObject
{
Q_OBJECT
public:
	Scheduler();
	virtual ~Scheduler();
	
	void reload();

	static void loadActions(QList<ScheduledAction>& list);
	static void saveActions(const QList<ScheduledAction>& list);
	static Scheduler* instance() { return m_instance; }
protected:
	void executeAction(const ScheduledAction& action);
private slots:
	void doWork();
private:
	QTimer m_timer;
	static Scheduler* m_instance;
	QList<ScheduledAction> m_actions;
};

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
	
	QString name;
	QUuid queue;
	ActionType action;
	QVariant actionArgument;
	QDateTime whenOneTime;
	QTime whenRepeated;
	bool daysRepeated[7];
	bool repeated;
};

#endif
