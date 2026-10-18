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

#ifndef TASKMANAGER_H
#define TASKMANAGER_H
#include <QList>
#include <QObject>
#include <QString>

class Task;

/* All plugin tasks, for the front ends to render.
 *
 * Main thread only. Finished tasks stay listed for a while, so their result
 * can still be seen, then they are dropped. Nothing is persisted.
 */
class TaskManager : public QObject
{
Q_OBJECT
public:
	static constexpr int DefaultRetentionMs = 5 * 60 * 1000;

	// Created on first use, which must be on the main thread.
	static TaskManager* instance();

	// Plugins go through TaskHost (TransferAction.h) rather than calling this.
	Task* create(const QString& plugin, const QString& title,
		     const QString& transferUuid = QString(), const QString& transferName = QString());
	QList<Task*> tasks() const { return m_tasks; }
	Task* task(const QString& id) const;

	// false if there is no such running task
	bool cancel(const QString& id);
	// Drops a finished task now; false if there is no such finished task
	bool dismiss(const QString& id);
	// Also run when the application is about to quit
	void cancelAll();

	void setRetention(int ms) { m_retentionMs = ms; } // for the tests
signals:
	void taskAdded(Task* task);
	void taskChanged(Task* task);
	void taskRemoved(const QString& id);
	void promptOpened(Task* task);
	void promptClosed(Task* task, const QString& promptId);
private:
	TaskManager();
	void remove(Task* task);

	QList<Task*> m_tasks;
	int m_retentionMs = DefaultRetentionMs;
};

#endif
