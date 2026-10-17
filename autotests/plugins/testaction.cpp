#include <QJsonArray>
#include <QJsonDocument>
#include <QStringList>
#include <QThread>

#include "fatrat.h"
#ifndef TESTACTION_LEGACY
#	include "plugins/PluginApi.h"
#endif

/* A plugin for the tests, and for trying the plugin API by hand:
 *
 *   PLUGIN_PATH=build/nogui/autotests build/nogui/fatrat-nogui
 *
 * It never dereferences the Transfer* it is given: the tests pass a stand-in.
 *
 * TESTACTION_LEGACY builds a plugin from before the plugin API (getInfo only),
 * TESTACTION_NEEDS_GUI one that says it needs the desktop interface, and
 * TESTACTION_API_VERSION one built for another API version.
 */

extern "C" __attribute__((visibility("default"))) PluginInfo getInfo()
{
	PluginInfo info;

	info.version = VERSION;
#ifdef TESTACTION_LEGACY
	info.name = "Test legacy plugin";
#else
	info.name = "Test action plugin";
#endif
	info.author = "FatRat tests";
	info.website = "https://fatrat.dolezel.info";
	return info;
}

#ifndef TESTACTION_LEGACY

#ifndef TESTACTION_API_VERSION
#	define TESTACTION_API_VERSION FATRAT_PLUGIN_API_VERSION
#endif

static QJsonObject json(const char* text)
{
	return QJsonDocument::fromJson(text).object();
}

// Asks one question, answered (or not) through a callback on the main thread.
static void greet(Transfer*, TaskHost& host)
{
	Task* task = host.createTask("Greeting");

	task->ask(json(R"({ "title": "Who are you?",
		"fields": [ { "type": "text", "key": "name", "title": "Name" } ] })"), 120000,
		[task](std::optional<QJsonObject> answer) {
			if (!answer)
				task->finish(false, "No answer");
			else
				task->finish(true, "Hello " + answer->value("name").toString());
		});
}

// Works on a thread of its own, reports progress, and asks halfway through.
static void count(Transfer*, TaskHost& host)
{
	Task* task = host.createTask("Counting to ten");

	QThread* worker = QThread::create([task] {
		for (int i = 1; i <= 10; i++)
		{
			if (task->cancelRequested())
			{
				task->finish(false, "Cancelled");
				return;
			}

			if (i == 5)
			{
				std::optional<QJsonObject> answer = task->askBlocking(json(R"({
					"title": "Halfway there",
					"description": "Pick some letters and a directory.",
					"fields": [
						{ "type": "checklist", "key": "letters", "title": "Letters", "columns": ["Letter", "Note"],
						  "rows": [ { "value": "a", "cells": ["A", "first"], "checked": true },
							    { "value": "b", "cells": ["B", "second"] },
							    { "value": "c", "cells": ["C", "third"] } ] },
						{ "type": "directory", "key": "dir", "title": "Directory", "default": "/tmp" },
						{ "type": "combo", "key": "speed", "title": "Speed",
						  "options": [ { "value": "slow", "title": "Slow" }, { "value": "fast", "title": "Fast" } ] }
					] })"), 300000);

				if (!answer)
				{
					task->finish(false, "No answer");
					return;
				}

				QStringList letters;
				for (const QJsonValue& v : answer->value("letters").toArray())
					letters << v.toString();
				task->setStatus(QString("Chose %1 in %2").arg(letters.join(','), answer->value("dir").toString()));
			}
			else
				task->setStatus(QString("Step %1").arg(i));

			task->setProgress(i * 10);
			QThread::sleep(1);
		}
		task->finish(true, "Counted to ten");
	});

	QObject::connect(worker, &QThread::finished, worker, &QObject::deleteLater);
	worker->start();
}

FATRAT_PLUGIN_EXPORT int getApiVersion()
{
	return TESTACTION_API_VERSION;
}

FATRAT_PLUGIN_EXPORT bool requiresGui()
{
#ifdef TESTACTION_NEEDS_GUI
	return true;
#else
	return false;
#endif
}

FATRAT_PLUGIN_EXPORT void initPlugin()
{
	TransferAction a;

	a.id = "test.greet";
	a.label = "Greet (test plugin)";
	a.icon = "dialog-question";
	a.trigger = greet;
	registerTransferAction(a);

	a.id = "test.count";
	a.label = "Count to ten (test plugin)";
	a.icon = "chronometer";
	a.trigger = count;
	registerTransferAction(a);

	a.id = "test.instant";
	a.label = "Do nothing (test plugin)";
	a.icon = QString();
	a.trigger = [](Transfer*, TaskHost&) {};
	registerTransferAction(a);
}

#endif // TESTACTION_LEGACY
