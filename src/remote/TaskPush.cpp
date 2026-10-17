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

#include "TaskPush.h"
#include "TasksJson.h"

#include "plugins/Task.h"
#include "plugins/TaskManager.h"

#include <QPointer>
#include <QTimer>

static QJsonObject withoutProgress(QJsonObject json)
{
	json.remove("progress");
	json.remove("status");
	return json;
}

TaskPush::TaskPush(std::function<void(const QJsonObject&)> send, QObject* parent)
	: QObject(parent), m_send(send)
{
	m_clock.start();

	TaskManager* manager = TaskManager::instance();
	connect(manager, &TaskManager::taskAdded, this, &TaskPush::changed);
	connect(manager, &TaskManager::taskChanged, this, &TaskPush::changed);
	connect(manager, &TaskManager::taskRemoved, this, &TaskPush::removed);
}

void TaskPush::changed(Task* task)
{
	QString id = task->id();
	QJsonObject json = TasksJson::task(task);
	Sent& sent = m_sent[id];
	qint64 since = m_clock.elapsed() - sent.at;

	if (json == sent.json)
		return;

	bool progressOnly = !sent.json.isEmpty() && withoutProgress(json) == withoutProgress(sent.json);

	if (progressOnly && since < ProgressIntervalMs)
	{
		if (!sent.pending)
		{
			QPointer<Task> guard(task);

			sent.pending = true;
			QTimer::singleShot(ProgressIntervalMs - since, this, [this, guard, id] {
				if (!guard || !m_sent.value(id).pending)
					return;

				// The task may have returned to what the client already has.
				QJsonObject now = TasksJson::task(guard);
				if (now == m_sent.value(id).json)
					m_sent[id].pending = false;
				else
					send(id, now);
			});
		}
		return;
	}

	send(id, json);
}

void TaskPush::send(const QString& id, const QJsonObject& json)
{
	Sent& sent = m_sent[id];
	QJsonObject message;

	sent.json = json;
	sent.at = m_clock.elapsed();
	sent.pending = false;

	message["type"] = "task";
	message["task"] = json;
	m_send(message);
}

void TaskPush::removed(const QString& id)
{
	QJsonObject message;

	m_sent.remove(id);
	message["type"] = "task-removed";
	message["id"] = id;
	m_send(message);
}
