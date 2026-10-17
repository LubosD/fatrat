#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>
#include <stdexcept>

#include "plugins/Task.h"
#include "plugins/TaskManager.h"
#include "plugins/TransferActions.h"
#include "remote/RestApi.h"
#include "remote/RestResources.h"
#include "remote/RestTasks.h"

/* /transfers/:transfer/actions and /tasks: status codes, JSON shape and the
 * error envelope. There are no queues here: one transfer, "T1", is faked
 * through the transfer visitor, and the Transfer* is never dereferenced. */

// The tests run on the main thread already.
void RestResources::onMainThread(const std::function<void()>& fn)
{
	fn();
}

static Transfer* fakeTransfer()
{
	static char storage;
	return reinterpret_cast<Transfer*>(&storage);
}

static QJsonObject nameForm()
{
	return QJsonDocument::fromJson(R"({ "title": "Who?", "fields": [ { "type": "text", "key": "name" } ] })").object();
}

static void reg(const QString& id, bool applies, std::function<void(Transfer*, TaskHost&)> trigger)
{
	TransferAction a;
	a.id = id;
	a.label = id + " label";
	a.icon = "system-run";
	a.appliesTo = [applies](Transfer*) { return applies; };
	a.trigger = trigger;
	registerTransferAction(a);
}

static void askName(Task* task, int timeoutMs)
{
	task->ask(nameForm(), timeoutMs, [task](std::optional<QJsonObject> a) {
		if (!a)
			task->finish(false, "No answer");
		else
			task->finish(true, "Hello " + a->value("name").toString());
	});
}

class tst_resttasks : public QObject
{
	Q_OBJECT
private slots:
	void initTestCase();

	void listsApplicableActions();
	void unknownTransferIs404();
	void actionErrors();
	void instantActionIs204();
	void askAndAnswer();
	void answerValidation();
	void expiredPromptIs410();
	void dismissingAPrompt();
	void deleteCancelsThenDismisses();
	void dismissingAnExpiredPromptIs410();
	void deletingAnUnknownTaskIs404();
	void throwingAfterCreatingATaskFailsIt();
	void answeringACancelledTaskIs409();
	void malformedAnswerBodyIs400();
private:
	RestResponse call(const QString& method, const QString& path, const QByteArray& body = QByteArray());
	QJsonObject object(const RestResponse& r) { return QJsonDocument::fromJson(r.body).object(); }
	QString errorCode(const RestResponse& r) { return object(r).value("error").toObject().value("code").toString(); }
	QString start(const QString& action); // POSTs the action, returns the task id
};

void tst_resttasks::initTestCase()
{
	RestApi::clearRoutes();
	RestResources::registerTasks();

	RestTasks::setTransferVisitor([](const QString& uuid, const RestTasks::TransferFn& fn) {
		if (uuid != "T1")
			throw RestError(404, "transfer_not_found", "Invalid transfer UUID");
		fn(fakeTransfer(), "file.bin");
	});

	TransferActions::clear();
	reg("t.ask", true, [](Transfer*, TaskHost& host) { askName(host.createTask("Ask"), 60000); });
	reg("t.short", true, [](Transfer*, TaskHost& host) { askName(host.createTask("Ask briefly"), 1); });
	reg("t.long", true, [](Transfer*, TaskHost& host) { host.createTask("Runs until stopped"); });
	reg("t.instant", true, [](Transfer*, TaskHost&) {});
	reg("t.never", false, [](Transfer*, TaskHost&) {});
	reg("t.throw", true, [](Transfer*, TaskHost&) { throw std::runtime_error("broken"); });
	reg("t.taskthrow", true, [](Transfer*, TaskHost& host) {
		host.createTask("Doomed");
		throw std::runtime_error("kaput");
	});
}

RestResponse tst_resttasks::call(const QString& method, const QString& path, const QByteArray& body)
{
	RestRequest request;

	request.method = method;
	request.path = QString("/api/v1") + path;
	request.body = body;
	request.contentType = "application/json";
	// The CSRF guard wants a same-origin request.
	request.host = "localhost";
	request.origin = "http://localhost";

	return RestApi::dispatch(request);
}

QString tst_resttasks::start(const QString& action)
{
	RestResponse r = call("POST", "/transfers/T1/actions/" + action);

	if (r.status != 202)
		qWarning() << "starting" << action << "gave" << r.status << r.body;
	return object(r).value("task").toString();
}

void tst_resttasks::listsApplicableActions()
{
	RestResponse r = call("GET", "/transfers/T1/actions");
	QCOMPARE(r.status, 200);

	QJsonArray list = QJsonDocument::fromJson(r.body).array();
	QStringList ids;
	for (const QJsonValue& v : list)
		ids << v.toObject().value("id").toString();

	QCOMPARE(ids, QStringList({ "t.ask", "t.short", "t.long", "t.instant", "t.throw", "t.taskthrow" }));
	QCOMPARE(list[0].toObject().value("label").toString(), QString("t.ask label"));
	QCOMPARE(list[0].toObject().value("icon").toString(), QString("system-run"));
}

void tst_resttasks::unknownTransferIs404()
{
	RestResponse r = call("GET", "/transfers/T9/actions");
	QCOMPARE(r.status, 404);
	QCOMPARE(errorCode(r), QString("transfer_not_found"));
}

void tst_resttasks::actionErrors()
{
	RestResponse r = call("POST", "/transfers/T1/actions/t.missing");
	QCOMPARE(r.status, 404);
	QCOMPARE(errorCode(r), QString("action_not_found"));

	r = call("POST", "/transfers/T1/actions/t.never");
	QCOMPARE(r.status, 409);
	QCOMPARE(errorCode(r), QString("not_applicable"));

	r = call("POST", "/transfers/T1/actions/t.throw");
	QCOMPARE(r.status, 500);
	QCOMPARE(errorCode(r), QString("plugin_error"));
}

void tst_resttasks::instantActionIs204()
{
	QCOMPARE(call("POST", "/transfers/T1/actions/t.instant").status, 204);
}

void tst_resttasks::askAndAnswer()
{
	QString id = start("t.ask");
	QVERIFY(!id.isEmpty());

	QJsonObject task = object(call("GET", "/tasks/" + id));
	QCOMPARE(task.value("state").toString(), QString("waiting"));
	QCOMPARE(task.value("plugin").toString(), QString("fatrat"));
	QCOMPARE(task.value("transfer").toString(), QString("T1"));
	QCOMPARE(task.value("transferName").toString(), QString("file.bin"));
	QCOMPARE(task.value("progress").toInt(), -1);
	QVERIFY(task.value("message").isNull());
	QJsonObject prompt = task.value("prompt").toObject();
	QCOMPARE(prompt.value("form").toObject().value("title").toString(), QString("Who?"));
	QVERIFY(!prompt.value("expiresAt").toString().isEmpty());
	QString pid = prompt.value("id").toString();

	bool listed = false;
	for (const QJsonValue& v : QJsonDocument::fromJson(call("GET", "/tasks").body).array())
		listed |= v.toObject().value("id").toString() == id;
	QVERIFY(listed);

	RestResponse r = call("POST", "/tasks/" + id + "/prompts/" + pid, R"({ "values": { "name": "Ann" } })");
	QCOMPARE(r.status, 200);
	QCOMPARE(object(r).value("state").toString(), QString("done"));
	QCOMPARE(object(r).value("message").toString(), QString("Hello Ann"));
	QVERIFY(!object(r).contains("prompt"));

	r = call("POST", "/tasks/" + id + "/prompts/" + pid, R"({ "values": { "name": "Bob" } })");
	QCOMPARE(r.status, 409);
	QCOMPARE(errorCode(r), QString("already_answered"));
}

void tst_resttasks::answerValidation()
{
	QString id = start("t.ask");
	QString pid = object(call("GET", "/tasks/" + id)).value("prompt").toObject().value("id").toString();

	RestResponse r = call("POST", "/tasks/" + id + "/prompts/" + pid, R"({ "values": { "name": 5 } })");
	QCOMPARE(r.status, 400);
	QCOMPARE(errorCode(r), QString("invalid_answer"));

	r = call("POST", "/tasks/" + id + "/prompts/" + pid, "{}");
	QCOMPARE(r.status, 400);

	r = call("POST", "/tasks/nonexistent/prompts/" + pid, R"({ "values": {} })");
	QCOMPARE(r.status, 404);
	QCOMPARE(errorCode(r), QString("task_not_found"));

	r = call("POST", "/tasks/" + id + "/prompts/nonexistent", R"({ "values": {} })");
	QCOMPARE(r.status, 404);
	QCOMPARE(errorCode(r), QString("prompt_not_found"));

	// Still open after all that.
	QCOMPARE(object(call("GET", "/tasks/" + id)).value("state").toString(), QString("waiting"));
}

void tst_resttasks::expiredPromptIs410()
{
	QString id = start("t.short");
	QString pid = object(call("GET", "/tasks/" + id)).value("prompt").toObject().value("id").toString();

	QTRY_COMPARE_WITH_TIMEOUT(object(call("GET", "/tasks/" + id)).value("state").toString(), QString("failed"), 3000);

	RestResponse r = call("POST", "/tasks/" + id + "/prompts/" + pid, R"({ "values": { "name": "late" } })");
	QCOMPARE(r.status, 410);
	QCOMPARE(errorCode(r), QString("expired"));
}

void tst_resttasks::dismissingAPrompt()
{
	QString id = start("t.ask");
	QString pid = object(call("GET", "/tasks/" + id)).value("prompt").toObject().value("id").toString();

	QCOMPARE(call("DELETE", "/tasks/" + id + "/prompts/" + pid).status, 204);

	QJsonObject task = object(call("GET", "/tasks/" + id));
	QCOMPARE(task.value("state").toString(), QString("failed"));
	QCOMPARE(task.value("message").toString(), QString("No answer"));
	QCOMPARE(call("DELETE", "/tasks/" + id + "/prompts/" + pid).status, 409);
}

void tst_resttasks::deleteCancelsThenDismisses()
{
	QString id = start("t.long");

	RestResponse r = call("DELETE", "/tasks/" + id);
	QCOMPARE(r.status, 202);
	QVERIFY(TaskManager::instance()->task(id)->cancelRequested());

	// t.long never stops by itself; play the plugin's part.
	TaskManager::instance()->task(id)->finish(false);
	QCOMPARE(object(call("GET", "/tasks/" + id)).value("state").toString(), QString("cancelled"));

	QCOMPARE(call("DELETE", "/tasks/" + id).status, 204);
	QCOMPARE(call("GET", "/tasks/" + id).status, 404);
}

void tst_resttasks::dismissingAnExpiredPromptIs410()
{
	QString id = start("t.short");
	QString pid = object(call("GET", "/tasks/" + id)).value("prompt").toObject().value("id").toString();

	QTRY_COMPARE_WITH_TIMEOUT(object(call("GET", "/tasks/" + id)).value("state").toString(), QString("failed"), 3000);

	RestResponse r = call("DELETE", "/tasks/" + id + "/prompts/" + pid);
	QCOMPARE(r.status, 410);
	QCOMPARE(errorCode(r), QString("expired"));
}

void tst_resttasks::deletingAnUnknownTaskIs404()
{
	RestResponse r = call("DELETE", "/tasks/unknown");
	QCOMPARE(r.status, 404);
	QCOMPARE(errorCode(r), QString("task_not_found"));
}

void tst_resttasks::throwingAfterCreatingATaskFailsIt()
{
	RestResponse r = call("POST", "/transfers/T1/actions/t.taskthrow");
	QCOMPARE(r.status, 500);
	QCOMPARE(errorCode(r), QString("plugin_error"));

	bool found = false;
	for (const QJsonValue& v : QJsonDocument::fromJson(call("GET", "/tasks").body).array())
	{
		QJsonObject t = v.toObject();
		if (t.value("title").toString() != "Doomed")
			continue;
		found = true;
		QCOMPARE(t.value("state").toString(), QString("failed"));
		QCOMPARE(t.value("message").toString(), QString("kaput"));
	}
	QVERIFY(found);
}

void tst_resttasks::answeringACancelledTaskIs409()
{
	QString id = start("t.ask");
	QString pid = object(call("GET", "/tasks/" + id)).value("prompt").toObject().value("id").toString();

	QCOMPARE(call("DELETE", "/tasks/" + id).status, 202);

	RestResponse r = call("POST", "/tasks/" + id + "/prompts/" + pid, R"({ "values": { "name": "Ann" } })");
	QCOMPARE(r.status, 409);
	QCOMPARE(errorCode(r), QString("already_answered"));
}

void tst_resttasks::malformedAnswerBodyIs400()
{
	QString id = start("t.ask");
	QString pid = object(call("GET", "/tasks/" + id)).value("prompt").toObject().value("id").toString();

	RestResponse r = call("POST", "/tasks/" + id + "/prompts/" + pid, "{ not json");
	QCOMPARE(r.status, 400);
	QCOMPARE(errorCode(r), QString("invalid_json"));
}

QTEST_GUILESS_MAIN(tst_resttasks)
#include "tst_resttasks.moc"
