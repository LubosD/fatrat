#include <QTest>
#include <QThread>
#include <stdexcept>

#include "plugins/Task.h"
#include "plugins/TransferActions.h"

/* The action registry. The Transfer* handed around is a stand-in that is never
 * dereferenced: the registry only passes it to the plugin, and the test
 * actions only compare it. */
static Transfer* fakeTransfer()
{
	static char storage;
	return reinterpret_cast<Transfer*>(&storage);
}

static Transfer* otherTransfer()
{
	static char storage;
	return reinterpret_cast<Transfer*>(&storage);
}

static TransferAction action(const QString& id, std::function<bool(Transfer*)> appliesTo,
			     std::function<void(Transfer*, TaskHost&)> trigger = nullptr)
{
	TransferAction a;
	a.id = id;
	a.label = id + " label";
	a.icon = "system-run";
	a.appliesTo = appliesTo;
	a.trigger = trigger;
	return a;
}

class tst_transferactions : public QObject
{
	Q_OBJECT
private slots:
	void init();
	void listsOnlyApplicableActions();
	void ignoresADuplicateId();
	void attributesToTheLoadingPlugin();
	void throwingAppliesToCountsAsFalse();
	void triggerCreatesAnAttributedTask();
	void triggerWithoutTask();
	void throwingTriggerFailsItsTask();
	void nonStdExceptionsAreCaught();
	void throwingTriggerFailsEveryTask();
	void registrationFromAnotherThreadIsIgnored();
};

void tst_transferactions::init()
{
	TransferActions::clear();
}

void tst_transferactions::listsOnlyApplicableActions()
{
	registerTransferAction(action("a.yes", [](Transfer*) { return true; }));
	registerTransferAction(action("a.mine", [](Transfer* t) { return t == fakeTransfer(); }));
	registerTransferAction(action("a.no", [](Transfer*) { return false; }));

	QList<TransferActions::Entry> list = TransferActions::applicable(fakeTransfer());
	QCOMPARE(list.size(), 2);
	QCOMPARE(list[0].action.id, QString("a.yes"));
	QCOMPARE(list[1].action.id, QString("a.mine"));
	QCOMPARE(TransferActions::applicable(otherTransfer()).size(), 1);

	QVERIFY(TransferActions::find("a.no").has_value());
	QVERIFY(!TransferActions::find("a.missing").has_value());
	QVERIFY(!TransferActions::applies(*TransferActions::find("a.no"), fakeTransfer()));
}

void tst_transferactions::ignoresADuplicateId()
{
	registerTransferAction(action("a.dup", [](Transfer*) { return true; }));
	TransferAction second = action("a.dup", [](Transfer*) { return true; });
	second.label = "second";
	registerTransferAction(second);

	QCOMPARE(TransferActions::applicable(fakeTransfer()).size(), 1);
	QCOMPARE(TransferActions::find("a.dup")->action.label, QString("a.dup label"));
}

void tst_transferactions::attributesToTheLoadingPlugin()
{
	registerTransferAction(action("a.builtin", nullptr));
	TransferActions::setLoadingPlugin("example.so");
	registerTransferAction(action("a.plugin", nullptr));
	TransferActions::setLoadingPlugin(QString());

	QCOMPARE(TransferActions::find("a.builtin")->plugin, QString("fatrat"));
	QCOMPARE(TransferActions::find("a.plugin")->plugin, QString("example.so"));
	// No appliesTo means it always applies.
	QCOMPARE(TransferActions::applicable(fakeTransfer()).size(), 2);
}

void tst_transferactions::throwingAppliesToCountsAsFalse()
{
	registerTransferAction(action("a.throws", [](Transfer*) -> bool { throw std::runtime_error("x"); }));
	QCOMPARE(TransferActions::applicable(fakeTransfer()).size(), 0);
}

void tst_transferactions::triggerCreatesAnAttributedTask()
{
	Transfer* seen = nullptr;

	TransferActions::setLoadingPlugin("example.so");
	registerTransferAction(action("a.task", nullptr, [&](Transfer* t, TaskHost& host) {
		seen = t;
		host.createTask("Doing it");
	}));
	TransferActions::setLoadingPlugin(QString());

	Task* task = TransferActions::trigger(*TransferActions::find("a.task"), fakeTransfer(), "uuid-7", "file.rar");

	QCOMPARE(seen, fakeTransfer());
	QVERIFY(task);
	QCOMPARE(task->plugin(), QString("example.so"));
	QCOMPARE(task->title(), QString("Doing it"));
	QCOMPARE(task->transferUuid(), QString("uuid-7"));
	QCOMPARE(task->transferName(), QString("file.rar"));
}

void tst_transferactions::triggerWithoutTask()
{
	bool ran = false;
	registerTransferAction(action("a.instant", nullptr, [&](Transfer*, TaskHost&) { ran = true; }));

	QCOMPARE(TransferActions::trigger(*TransferActions::find("a.instant"), fakeTransfer(), "u", "n"), nullptr);
	QVERIFY(ran);
}

void tst_transferactions::throwingTriggerFailsItsTask()
{
	Task* created = nullptr;
	registerTransferAction(action("a.fails", nullptr, [&](Transfer*, TaskHost& host) {
		created = host.createTask("Doomed");
		throw std::runtime_error("broken");
	}));

	bool threw = false;
	try
	{
		TransferActions::trigger(*TransferActions::find("a.fails"), fakeTransfer(), "u", "n");
	}
	catch (const PluginError& e)
	{
		threw = true;
		QCOMPARE(e.message, QString("broken"));
		QCOMPARE(e.plugin, QString("fatrat"));
	}

	QVERIFY(threw);
	QVERIFY(created);
	QCOMPARE(created->state(), Task::Failed);
}

void tst_transferactions::nonStdExceptionsAreCaught()
{
	registerTransferAction(action("a.int", [](Transfer*) -> bool { throw 42; }));
	QCOMPARE(TransferActions::applicable(fakeTransfer()).size(), 0);

	Task* created = nullptr;
	registerTransferAction(action("a.int2", nullptr, [&](Transfer*, TaskHost& host) {
		created = host.createTask("Doomed");
		throw 42;
	}));

	bool threw = false;
	try
	{
		TransferActions::trigger(*TransferActions::find("a.int2"), fakeTransfer(), "u", "n");
	}
	catch (const PluginError& e)
	{
		threw = true;
		QCOMPARE(e.message, QString("unknown error"));
	}

	QVERIFY(threw);
	QVERIFY(created);
	QCOMPARE(created->state(), Task::Failed);
}

void tst_transferactions::throwingTriggerFailsEveryTask()
{
	Task* first = nullptr;
	Task* second = nullptr;
	registerTransferAction(action("a.two", nullptr, [&](Transfer*, TaskHost& host) {
		first = host.createTask("One");
		second = host.createTask("Two");
		QCOMPARE(host.createdTask(), first);
		QCOMPARE(host.tasks().size(), 2);
		throw std::runtime_error("broken");
	}));

	bool threw = false;
	try
	{
		TransferActions::trigger(*TransferActions::find("a.two"), fakeTransfer(), "u", "n");
	}
	catch (const PluginError&)
	{
		threw = true;
	}
	QVERIFY(threw);
	QVERIFY(first && second);
	QCOMPARE(first->state(), Task::Failed);
	QCOMPARE(second->state(), Task::Failed);
}

void tst_transferactions::registrationFromAnotherThreadIsIgnored()
{
	QThread* thread = QThread::create([] {
		registerTransferAction(action("a.thread", nullptr));
	});
	thread->start();
	QVERIFY(thread->wait(5000));
	delete thread;

	QVERIFY(!TransferActions::find("a.thread").has_value());
}

QTEST_GUILESS_MAIN(tst_transferactions)
#include "tst_transferactions.moc"
