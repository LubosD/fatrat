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


#include "RestResources.h"
#include "SchedulerJson.h"
#include "Scheduler.h"
#include "Queue.h"
#include <QReadLocker>

using RestResources::onMainThread;

static bool queueExists(const QString& uuid)
{
	QReadLocker l(&g_queuesLock);

	for (Queue* q : g_queues)
	{
		if (q->uuid() == uuid)
			return true;
	}
	return false;
}

static int findAction(const QList<ScheduledAction>& list, const QString& id)
{
	for (int i = 0; i < list.size(); i++)
	{
		if (list[i].id == id)
			return i;
	}
	throw RestError(404, "action_not_found", "Invalid scheduled action id");
}

// The Scheduler keeps its own copy of the list, so it has to be told about every change
static void save(const QList<ScheduledAction>& list)
{
	ScheduledAction::saveAll(list);
	Scheduler::instance()->reload();
}

static RestResult listActions(const RestRequest&)
{
	QJsonArray rv;

	onMainThread([&] {
		QList<ScheduledAction> list;
		ScheduledAction::loadAll(list);
		for (const ScheduledAction& a : list)
			rv << SchedulerJson::actionToJson(a);
	});
	return RestResult::ok(rv);
}

static RestResult createAction(const RestRequest& req)
{
	const QJsonObject body = req.jsonObject();
	QJsonObject rv;

	onMainThread([&] {
		QList<ScheduledAction> list;
		ScheduledAction::loadAll(list);

		ScheduledAction a;
		a.id = ScheduledAction::newId();
		SchedulerJson::applyAction(a, body, queueExists, true);

		list << a;
		save(list);
		rv = SchedulerJson::actionToJson(a);
	});
	return RestResult::created(rv, QString("%1/scheduler/actions/%2").arg(RestApi::Prefix, rv["id"].toString()));
}

static RestResult patchAction(const RestRequest& req)
{
	const QJsonObject body = req.jsonObject();
	const QString id = req.pathParams["action"];
	QJsonObject rv;

	onMainThread([&] {
		QList<ScheduledAction> list;
		ScheduledAction::loadAll(list);

		ScheduledAction& a = list[findAction(list, id)];
		SchedulerJson::applyAction(a, body, queueExists, false);

		save(list);
		rv = SchedulerJson::actionToJson(a);
	});
	return RestResult::ok(rv);
}

static RestResult deleteAction(const RestRequest& req)
{
	const QString id = req.pathParams["action"];

	onMainThread([&] {
		QList<ScheduledAction> list;
		ScheduledAction::loadAll(list);
		list.removeAt(findAction(list, id));
		save(list);
	});
	return RestResult::noContent();
}

void RestResources::registerScheduler()
{
	RestApi::registerRoute("GET", "/scheduler/actions", listActions);
	RestApi::registerRoute("POST", "/scheduler/actions", createAction);
	RestApi::registerRoute("PATCH", "/scheduler/actions/:action", patchAction);
	RestApi::registerRoute("DELETE", "/scheduler/actions/:action", deleteAction);
}
