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

#include "TasksJson.h"

static QJsonValue orNull(const QString& s)
{
	return s.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(s);
}

QString TasksJson::stateName(Task::State state)
{
	switch (state)
	{
	case Task::Running:
		return "running";
	case Task::Waiting:
		return "waiting";
	case Task::Done:
		return "done";
	case Task::Failed:
		return "failed";
	case Task::Cancelled:
		return "cancelled";
	}
	return "running";
}

QJsonObject TasksJson::task(const Task* task)
{
	QJsonObject o;

	o["id"] = task->id();
	o["plugin"] = task->plugin();
	o["title"] = task->title();
	o["transfer"] = orNull(task->transferUuid());
	o["transferName"] = orNull(task->transferName());
	o["state"] = stateName(task->state());
	o["progress"] = task->progress();
	o["status"] = task->status();
	o["message"] = orNull(task->message());

	if (std::optional<Task::Prompt> p = task->prompt())
	{
		QJsonObject prompt;

		prompt["id"] = p->id;
		prompt["form"] = p->form;
		prompt["expiresAt"] = p->expiresAt.toString(Qt::ISODateWithMs);
		o["prompt"] = prompt;
	}
	return o;
}

QJsonObject TasksJson::action(const TransferAction& action)
{
	QJsonObject o;

	o["id"] = action.id;
	o["label"] = action.label;
	o["icon"] = action.icon;
	return o;
}
