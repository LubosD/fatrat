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

#include "RestTasks.h"
#include "RestApi.h"
#include "RestResources.h"
#include "TasksJson.h"

#include "plugins/Task.h"
#include "plugins/TaskManager.h"
#include "plugins/TransferActions.h"

#include <QJsonArray>

/* Plugin actions and their tasks.
 *
 * Everything here touches TaskManager or a plugin, so it runs on the main
 * thread. Answers come in over REST rather than the socket: REST already has
 * authentication, validation and error codes.
 */

using RestResources::onMainThread;

static RestTasks::TransferVisitor s_visitor;

void RestTasks::setTransferVisitor(const TransferVisitor& visitor)
{
	s_visitor = visitor;
}

// Main thread
static Task* findTask(const QString& id)
{
	Task* task = TaskManager::instance()->task(id);

	if (!task)
		throw RestError(404, "task_not_found", "No such task");
	return task;
}

static void checkPromptResult(Task::AnswerResult result, const QString& error)
{
	switch (result)
	{
	case Task::Answered:
		return;
	case Task::Invalid:
		throw RestError(400, "invalid_answer", error);
	case Task::AlreadyAnswered:
		throw RestError(409, "already_answered", "The question is no longer open");
	case Task::Expired:
		throw RestError(410, "expired", "The question has expired");
	case Task::UnknownPrompt:
		throw RestError(404, "prompt_not_found", "No such question");
	}
}

static RestResult listActions(const RestRequest& req)
{
	QJsonArray rv;

	s_visitor(req.pathParams.value("transfer"), [&](Transfer* t, const QString&) {
		for (const TransferActions::Entry& e : TransferActions::applicable(t))
			rv << TasksJson::action(e.action);
	});
	return RestResult::ok(rv);
}

static RestResult runAction(const RestRequest& req)
{
	QString uuid = req.pathParams.value("transfer");
	QString id = req.pathParams.value("action");
	QString taskId;

	s_visitor(uuid, [&](Transfer* t, const QString& name) {
		std::optional<TransferActions::Entry> entry = TransferActions::find(id);

		if (!entry)
			throw RestError(404, "action_not_found", "No such action");
		// The menu the user picked it from may be stale.
		if (!TransferActions::applies(*entry, t))
			throw RestError(409, "not_applicable", "The action does not apply to this transfer");

		try
		{
			if (Task* task = TransferActions::trigger(*entry, t, uuid, name))
				taskId = task->id();
		}
		catch (const PluginError& e)
		{
			throw RestError(500, "plugin_error", e.message);
		}
	});

	if (taskId.isEmpty())
		return RestResult::noContent();

	QJsonObject body;
	body["task"] = taskId;
	return RestResult::accepted(body);
}

static RestResult listTasks(const RestRequest&)
{
	QJsonArray rv;

	onMainThread([&] {
		for (Task* task : TaskManager::instance()->tasks())
			rv << TasksJson::task(task);
	});
	return RestResult::ok(rv);
}

static RestResult getTask(const RestRequest& req)
{
	QJsonObject rv;

	onMainThread([&] { rv = TasksJson::task(findTask(req.pathParams.value("task"))); });
	return RestResult::ok(rv);
}

static RestResult deleteTask(const RestRequest& req)
{
	QString id = req.pathParams.value("task");
	bool finished = false;
	QJsonObject rv;

	onMainThread([&] {
		Task* task = findTask(id);

		finished = task->isFinished();
		if (finished)
			TaskManager::instance()->dismiss(id);
		else
		{
			task->requestCancel();
			rv = TasksJson::task(task);
		}
	});
	return finished ? RestResult::noContent() : RestResult::accepted(rv);
}

static RestResult answerPrompt(const RestRequest& req)
{
	QJsonValue values = req.jsonObject().value("values");
	QJsonObject rv;

	if (!values.isObject())
		throw RestError(400, "invalid_answer", "\"values\" must be an object");

	onMainThread([&] {
		Task* task = findTask(req.pathParams.value("task"));
		QString error;

		checkPromptResult(task->answer(req.pathParams.value("prompt"), values.toObject(), &error), error);
		rv = TasksJson::task(task);
	});
	return RestResult::ok(rv);
}

static RestResult dismissPrompt(const RestRequest& req)
{
	onMainThread([&] {
		Task* task = findTask(req.pathParams.value("task"));
		checkPromptResult(task->dismissPrompt(req.pathParams.value("prompt")), QString());
	});
	return RestResult::noContent();
}

void RestResources::registerTasks()
{
	RestApi::registerRoute("GET", "/transfers/:transfer/actions", listActions);
	RestApi::registerRoute("POST", "/transfers/:transfer/actions/:action", runAction);

	RestApi::registerRoute("GET", "/tasks", listTasks);
	RestApi::registerRoute("GET", "/tasks/:task", getTask);
	RestApi::registerRoute("DELETE", "/tasks/:task", deleteTask);
	RestApi::registerRoute("POST", "/tasks/:task/prompts/:prompt", answerPrompt);
	RestApi::registerRoute("DELETE", "/tasks/:task/prompts/:prompt", dismissPrompt);
}
