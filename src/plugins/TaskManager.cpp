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

#include "TaskManager.h"
#include "Task.h"

#include <QPointer>
#include <QThread>
#include <QTimer>
#include <memory>

TaskManager* TaskManager::instance()
{
	static TaskManager* manager = new TaskManager;
	return manager;
}

Task* TaskManager::create(const QString& plugin, const QString& title,
			  const QString& transferUuid, const QString& transferName)
{
	Q_ASSERT(QThread::currentThread() == thread());

	Task* task = new Task(plugin, title, transferUuid, transferName, this);
	QPointer<Task> guard(task);
	auto removalScheduled = std::make_shared<bool>(false);

	m_tasks << task;

	// Task::changed may come from a worker thread; `this` as the context makes it queued then.
	connect(task, &Task::changed, this, [this, guard, removalScheduled] {
		if (!guard)
			return;
		emit taskChanged(guard);

		if (guard->isFinished() && !*removalScheduled)
		{
			*removalScheduled = true;
			QTimer::singleShot(m_retentionMs, this, [this, guard] {
				if (guard)
					remove(guard);
			});
		}
	});
	connect(task, &Task::promptOpened, this, [this, guard] {
		if (guard)
			emit promptOpened(guard);
	});
	connect(task, &Task::promptClosed, this, [this, guard](const QString& promptId) {
		if (guard)
			emit promptClosed(guard, promptId);
	});

	emit taskAdded(task);
	return task;
}

Task* TaskManager::task(const QString& id) const
{
	for (Task* t : m_tasks)
	{
		if (t->id() == id)
			return t;
	}
	return nullptr;
}

bool TaskManager::cancel(const QString& id)
{
	Task* t = task(id);

	if (!t || t->isFinished())
		return false;
	t->requestCancel();
	return true;
}

bool TaskManager::dismiss(const QString& id)
{
	Task* t = task(id);

	if (!t || !t->isFinished())
		return false;
	remove(t);
	return true;
}

void TaskManager::remove(Task* task)
{
	if (!m_tasks.removeOne(task))
		return;

	QString id = task->id();
	task->deleteLater();
	emit taskRemoved(id);
}
