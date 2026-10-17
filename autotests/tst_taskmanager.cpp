#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTest>
#include <QScopeGuard>
#include <QThread>
#include <atomic>
#include <stdexcept>

#include "plugins/Task.h"
#include "plugins/TaskManager.h"

/* A task's life: progress, a question and its answer (or none), timeouts,
 * cancelling, and being dropped once finished. */
class tst_taskmanager : public QObject
{
	Q_OBJECT
private slots:
	// Safety net for prompts a test leaves open. Tests whose callbacks capture locals
	// must close the prompt themselves: cleanup() runs after their frame is gone.
	void cleanup();
	void reportsProgressStatusAndFinish();
	void callbackGetsTheNormalizedAnswer();
	void invalidAnswerKeepsThePromptOpen();
	void promptTimesOut();
	void dismissingDeliversNothing();
	void invalidFormIsRefusedAtOnce();
	void secondQuestionIsRefused();
	void askBlockingFromAWorkerThread();
	void cancelClosesThePromptAndEndsCancelled();
	void callbackExceptionFailsTheTask();
	void finishedTasksAreDroppedAfterRetention();
	void managerForwardsSignals();
	void askBlockingOnTheMainThreadReturnsAtOnce();
	void askFromAWorkerThreadHopsToTheMainThread();
	void cancelAllCancelsRunningTasks();
	void finishWithOpenPromptClosesIt();
	void answerThenDismissSecondIsAlreadyAnswered();
	void finishedTaskKeepsUnknownAndExpiredDistinct();
	void askBlockingAfterCancelReturnsAtOnce();
	void cancelWakesAnAskerWhoseQuestionNeverRan();
	void finishWakesAnAskerWhoseQuestionNeverRan();
};

void tst_taskmanager::cleanup()
{
	TaskManager::instance()->cancelAll();
	QCoreApplication::processEvents();
}

static QJsonObject nameForm()
{
	return QJsonDocument::fromJson(R"({ "title": "Who?", "fields": [ { "type": "text", "key": "name" } ] })").object();
}

static Task* newTask()
{
	return TaskManager::instance()->create("test.so", "A task", "uuid-1", "file.bin");
}

void tst_taskmanager::reportsProgressStatusAndFinish()
{
	Task* task = newTask();

	QCOMPARE(task->state(), Task::Running);
	QCOMPARE(task->progress(), -1);
	QCOMPARE(task->transferUuid(), QString("uuid-1"));
	QCOMPARE(task->transferName(), QString("file.bin"));

	task->setProgress(42);
	task->setStatus("Working");
	QCOMPARE(task->progress(), 42);
	QCOMPARE(task->status(), QString("Working"));

	task->finish(true, "All done");
	QCOMPARE(task->state(), Task::Done);
	QCOMPARE(task->progress(), 100);
	QCOMPARE(task->message(), QString("All done"));
	QVERIFY(task->isFinished());
}

void tst_taskmanager::callbackGetsTheNormalizedAnswer()
{
	Task* task = newTask();
	std::optional<QJsonObject> got;
	bool called = false;

	task->ask(nameForm(), 60000, [&](std::optional<QJsonObject> r) { called = true; got = r; });

	QCOMPARE(task->state(), Task::Waiting);
	QVERIFY(task->prompt().has_value());
	QString id = task->prompt()->id;
	QVERIFY(task->prompt()->expiresAt > QDateTime::currentDateTimeUtc());

	QCOMPARE(task->answer(id, QJsonDocument::fromJson(R"({ "name": "Ann", "junk": 1 })").object()), Task::Answered);
	QVERIFY(called);
	QVERIFY(got.has_value());
	QCOMPARE(got->value("name").toString(), QString("Ann"));
	QVERIFY(!got->contains("junk"));
	QCOMPARE(task->state(), Task::Running);
	QVERIFY(!task->prompt().has_value());

	QCOMPARE(task->answer(id, QJsonDocument::fromJson(R"({ "name": "Bob" })").object()), Task::AlreadyAnswered);
	QCOMPARE(task->answer("no-such-prompt", QJsonObject()), Task::UnknownPrompt);
}

void tst_taskmanager::invalidAnswerKeepsThePromptOpen()
{
	Task* task = newTask();
	bool called = false;

	task->ask(nameForm(), 60000, [&](std::optional<QJsonObject>) { called = true; });

	QString error;
	QCOMPARE(task->answer(task->prompt()->id, QJsonDocument::fromJson(R"({ "name": 5 })").object(), &error), Task::Invalid);
	QVERIFY(!error.isEmpty());
	QVERIFY(!called);
	QVERIFY(task->prompt().has_value());

	// The callback captures `called`, which dies with this function; cleanup() would be too late.
	task->dismissPrompt(task->prompt()->id);
}

void tst_taskmanager::promptTimesOut()
{
	Task* task = newTask();
	bool called = false;
	std::optional<QJsonObject> got = QJsonObject();

	// Clamped up to Task::MinTimeoutMs.
	task->ask(nameForm(), 1, [&](std::optional<QJsonObject> r) { called = true; got = r; });
	QString id = task->prompt()->id;

	QTRY_VERIFY_WITH_TIMEOUT(called, 3000);
	QVERIFY(!got.has_value());
	QCOMPARE(task->answer(id, QJsonDocument::fromJson(R"({ "name": "late" })").object()), Task::Expired);
}

void tst_taskmanager::dismissingDeliversNothing()
{
	Task* task = newTask();
	bool called = false;
	std::optional<QJsonObject> got = QJsonObject();

	task->ask(nameForm(), 60000, [&](std::optional<QJsonObject> r) { called = true; got = r; });
	QString id = task->prompt()->id;

	QCOMPARE(task->dismissPrompt(id), Task::Answered);
	QVERIFY(called);
	QVERIFY(!got.has_value());
	QCOMPARE(task->answer(id, QJsonDocument::fromJson(R"({ "name": "x" })").object()), Task::AlreadyAnswered);
}

void tst_taskmanager::invalidFormIsRefusedAtOnce()
{
	Task* task = newTask();
	bool called = false;
	std::optional<QJsonObject> got = QJsonObject();

	task->ask(QJsonDocument::fromJson(R"({ "fields": [ { "type": "bogus" } ] })").object(), 60000,
		[&](std::optional<QJsonObject> r) { called = true; got = r; });

	QVERIFY(called);
	QVERIFY(!got.has_value());
	QVERIFY(!task->prompt().has_value());
}

void tst_taskmanager::secondQuestionIsRefused()
{
	Task* task = newTask();
	bool secondCalled = false;
	std::optional<QJsonObject> second = QJsonObject();

	task->ask(nameForm(), 60000, [](std::optional<QJsonObject>) {});
	QString first = task->prompt()->id;
	task->ask(nameForm(), 60000, [&](std::optional<QJsonObject> r) { secondCalled = true; second = r; });

	QVERIFY(secondCalled);
	QVERIFY(!second.has_value());
	QCOMPARE(task->prompt()->id, first);
}

void tst_taskmanager::askBlockingFromAWorkerThread()
{
	Task* task = newTask();
	std::optional<QJsonObject> got;

	QThread* worker = QThread::create([&] { got = task->askBlocking(nameForm(), 60000); });
	// A failing QTRY must not leave the worker pointing at dead stack variables.
	auto cleanup = qScopeGuard([&] {
		if (worker && worker->isRunning())
		{
			if (task->prompt().has_value())
				task->dismissPrompt(task->prompt()->id);
			worker->wait(5000);
		}
	});
	worker->start();

	QTRY_VERIFY(task->prompt().has_value());
	QCOMPARE(task->answer(task->prompt()->id, QJsonDocument::fromJson(R"({ "name": "Cy" })").object()), Task::Answered);

	QVERIFY(worker->wait(5000));
	delete worker;
	worker = nullptr;
	QVERIFY(got.has_value());
	QCOMPARE(got->value("name").toString(), QString("Cy"));
}

void tst_taskmanager::cancelClosesThePromptAndEndsCancelled()
{
	Task* task = newTask();
	QSignalSpy cancelled(task, &Task::cancelled);
	bool called = false;
	std::optional<QJsonObject> got = QJsonObject();

	task->ask(nameForm(), 60000, [&](std::optional<QJsonObject> r) { called = true; got = r; });
	task->requestCancel();

	QCOMPARE(cancelled.count(), 1);
	QVERIFY(task->cancelRequested());
	QVERIFY(called);
	QVERIFY(!got.has_value());
	QCOMPARE(task->state(), Task::Running); // until the plugin stops

	task->finish(false, "Stopped");
	QCOMPARE(task->state(), Task::Cancelled);
}

void tst_taskmanager::callbackExceptionFailsTheTask()
{
	Task* task = newTask();

	task->ask(nameForm(), 60000, [](std::optional<QJsonObject>) { throw std::runtime_error("boom"); });
	task->answer(task->prompt()->id, QJsonDocument::fromJson(R"({ "name": "x" })").object());

	QCOMPARE(task->state(), Task::Failed);
	QCOMPARE(task->message(), QString("boom"));
}

void tst_taskmanager::finishedTasksAreDroppedAfterRetention()
{
	TaskManager* manager = TaskManager::instance();
	QSignalSpy removed(manager, &TaskManager::taskRemoved);

	manager->setRetention(50);
	Task* task = newTask();
	QString id = task->id();
	QCOMPARE(manager->task(id), task);

	task->finish(true);
	QTRY_COMPARE(removed.count(), 1);
	QCOMPARE(removed.at(0).at(0).toString(), id);
	QCOMPARE(manager->task(id), nullptr);
	manager->setRetention(TaskManager::DefaultRetentionMs);
}

void tst_taskmanager::managerForwardsSignals()
{
	TaskManager* manager = TaskManager::instance();
	QSignalSpy added(manager, &TaskManager::taskAdded);
	QSignalSpy changed(manager, &TaskManager::taskChanged);
	QSignalSpy opened(manager, &TaskManager::promptOpened);
	QSignalSpy closed(manager, &TaskManager::promptClosed);

	Task* task = newTask();
	QCOMPARE(added.count(), 1);

	task->setProgress(10);
	QTRY_VERIFY(changed.count() >= 1);

	task->ask(nameForm(), 60000, [](std::optional<QJsonObject>) {});
	QCOMPARE(opened.count(), 1);
	QString id = task->prompt()->id;
	task->dismissPrompt(id);
	QCOMPARE(closed.count(), 1);
	QCOMPARE(closed.at(0).at(1).toString(), id);

	QVERIFY(manager->cancel(task->id()));
	QVERIFY(!manager->dismiss(task->id())); // still running
	task->finish(false);
	QVERIFY(manager->dismiss(task->id()));
	QCOMPARE(manager->task(task->id()), nullptr);
}

void tst_taskmanager::askBlockingOnTheMainThreadReturnsAtOnce()
{
	Task* task = newTask();

	QVERIFY(!task->askBlocking(nameForm(), 60000).has_value());
	QVERIFY(!task->prompt().has_value());
}

void tst_taskmanager::askFromAWorkerThreadHopsToTheMainThread()
{
	Task* task = newTask();
	std::optional<QJsonObject> got;
	bool called = false;

	QThread* worker = QThread::create([&] {
		task->ask(nameForm(), 60000, [&](std::optional<QJsonObject> r) { got = r; called = true; });
	});
	worker->start();
	QVERIFY(worker->wait(5000));
	delete worker;

	QTRY_VERIFY(task->prompt().has_value());
	QCOMPARE(task->answer(task->prompt()->id, QJsonDocument::fromJson(R"({ "name": "Di" })").object()), Task::Answered);
	QVERIFY(called);
	QCOMPARE(got->value("name").toString(), QString("Di"));
}

void tst_taskmanager::cancelAllCancelsRunningTasks()
{
	Task* task = newTask();
	bool called = false;
	std::optional<QJsonObject> got = QJsonObject();

	task->ask(nameForm(), 60000, [&](std::optional<QJsonObject> r) { called = true; got = r; });
	TaskManager::instance()->cancelAll();

	QVERIFY(task->cancelRequested());
	QVERIFY(called);
	QVERIFY(!got.has_value());
}

void tst_taskmanager::finishWithOpenPromptClosesIt()
{
	Task* task = newTask();
	bool called = false;
	std::optional<QJsonObject> got = QJsonObject();

	task->ask(nameForm(), 60000, [&](std::optional<QJsonObject> r) { called = true; got = r; });
	QString id = task->prompt()->id;
	task->finish(true);

	QCOMPARE(task->answer(id, QJsonDocument::fromJson(R"({ "name": "x" })").object()), Task::AlreadyAnswered);
	QTRY_VERIFY(called);
	QVERIFY(!got.has_value());
}

void tst_taskmanager::answerThenDismissSecondIsAlreadyAnswered()
{
	Task* task = newTask();

	task->ask(nameForm(), 60000, [](std::optional<QJsonObject>) {});
	QString id = task->prompt()->id;
	QCOMPARE(task->answer(id, QJsonDocument::fromJson(R"({ "name": "x" })").object()), Task::Answered);
	QCOMPARE(task->dismissPrompt(id), Task::AlreadyAnswered);

	task->ask(nameForm(), 60000, [](std::optional<QJsonObject>) {});
	id = task->prompt()->id;
	QCOMPARE(task->dismissPrompt(id), Task::Answered);
	QCOMPARE(task->answer(id, QJsonDocument::fromJson(R"({ "name": "x" })").object()), Task::AlreadyAnswered);
}

void tst_taskmanager::finishedTaskKeepsUnknownAndExpiredDistinct()
{
	Task* task = newTask();
	bool called = false;

	task->ask(nameForm(), 1, [&](std::optional<QJsonObject>) { called = true; });
	QString id = task->prompt()->id;
	QTRY_VERIFY_WITH_TIMEOUT(called, 3000);
	task->finish(true);

	QJsonObject values = QJsonDocument::fromJson(R"({ "name": "x" })").object();
	QCOMPARE(task->answer(id, values), Task::Expired);
	QCOMPARE(task->answer("no-such-prompt", values), Task::UnknownPrompt);
	QCOMPARE(task->dismissPrompt("no-such-prompt"), Task::UnknownPrompt);
}

void tst_taskmanager::askBlockingAfterCancelReturnsAtOnce()
{
	Task* task = newTask();
	std::optional<QJsonObject> got = QJsonObject();

	task->requestCancel();
	QThread* worker = QThread::create([&] { got = task->askBlocking(nameForm(), 60000); });
	worker->start();
	QVERIFY(worker->wait(3000));
	delete worker;

	QVERIFY(!got.has_value());
	QVERIFY(!task->prompt().has_value());
}

// The main thread is not running its event loop (at quit, a plugin may be joining
// its workers), so the queued ask() cannot run and cancelling must wake the worker itself.
void tst_taskmanager::cancelWakesAnAskerWhoseQuestionNeverRan()
{
	Task* task = newTask();
	std::optional<QJsonObject> got = QJsonObject();
	std::atomic<bool> entered(false);

	QThread* worker = QThread::create([&] { entered = true; got = task->askBlocking(nameForm(), 60000); });
	worker->start();
	while (!entered)
		QThread::msleep(5);
	QThread::msleep(100);

	task->requestCancel(); // no events processed since the worker started
	QVERIFY(worker->wait(2000));
	delete worker;
	QVERIFY(!got.has_value());

	// The late queued ask() must not open a prompt on a cancelled task.
	QCoreApplication::processEvents();
	QVERIFY(!task->prompt().has_value());
}

void tst_taskmanager::finishWakesAnAskerWhoseQuestionNeverRan()
{
	Task* task = newTask();
	std::optional<QJsonObject> got = QJsonObject();
	std::atomic<bool> entered(false);

	QThread* worker = QThread::create([&] { entered = true; got = task->askBlocking(nameForm(), 60000); });
	worker->start();
	while (!entered)
		QThread::msleep(5);
	QThread::msleep(100);

	task->finish(false, "gave up");
	QVERIFY(worker->wait(2000));
	delete worker;
	QVERIFY(!got.has_value());

	QCoreApplication::processEvents();
	QVERIFY(!task->prompt().has_value());
}

QTEST_GUILESS_MAIN(tst_taskmanager)
#include "tst_taskmanager.moc"
