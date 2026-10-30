#include <QtTest>
#include <QSettings>
#include <QTemporaryDir>
#include "remote/SchedulerJson.h"

QSettings* g_settings = nullptr;

class TestSchedulerResources : public QObject
{
	Q_OBJECT
private slots:
	void initTestCase();
	void init();

	void idsAreGeneratedAndPersisted();
	void saveAndLoadRoundTrip();

	void createRepeated();
	void createSpeedLimit();
	void createOneTime();
	void createValidation();
	void patchIsPartial();
	void patchSwitchesActionAndSchedule();

	void oneTimeRunsOnce();
	void doneIsPersistedAndRearmed();
	void everyMinuteIsHandledOnce();
	void removeForQueueDropsOnlyThatQueue();
private:
	QTemporaryDir m_tmp;
};

static const QString Q1 = "{11111111-1111-1111-1111-111111111111}";

static bool queues(const QString& uuid)
{
	return uuid == Q1;
}

static int failure(const std::function<void()>& fn)
{
	try { fn(); }
	catch (const RestError& e) { return e.status; }
	return 0;
}

static QJsonObject obj(const char* json)
{
	return QJsonDocument::fromJson(json).object();
}

static ScheduledAction create(const char* json)
{
	ScheduledAction a;
	SchedulerJson::applyAction(a, obj(json), queues, true);
	return a;
}

void TestSchedulerResources::initTestCase()
{
	QVERIFY(m_tmp.isValid());
}

void TestSchedulerResources::init()
{
	delete g_settings;
	g_settings = new QSettings(m_tmp.filePath("test.ini"), QSettings::IniFormat);
	g_settings->clear();
}

void TestSchedulerResources::idsAreGeneratedAndPersisted()
{
	// a configuration written before ids existed
	g_settings->beginWriteArray("scheduler/actions", 2);
	for (int i = 0; i < 2; i++)
	{
		g_settings->setArrayIndex(i);
		g_settings->setValue("name", QString("old%1").arg(i));
		g_settings->setValue("queueUUID", Q1);
		g_settings->setValue("action", 1);
		g_settings->setValue("repeated", true);
		g_settings->setValue("whenRepeated", "03:00:00");
		g_settings->setValue("daysRepeated", "1111100");
	}
	g_settings->endArray();

	QList<ScheduledAction> a, b;
	ScheduledAction::loadAll(a);
	ScheduledAction::loadAll(b);

	QCOMPARE(a.size(), 2);
	QVERIFY(!a[0].id.isEmpty());
	QVERIFY(a[0].id != a[1].id);
	// the ids were saved, so the second load sees the same ones
	QCOMPARE(b[0].id, a[0].id);
	QCOMPARE(b[1].id, a[1].id);
	QCOMPARE(a[0].action, ScheduledAction::ActionStopAll);
	QVERIFY(a[0].daysRepeated[4]);
	QVERIFY(!a[0].daysRepeated[5]);
}

void TestSchedulerResources::saveAndLoadRoundTrip()
{
	QList<ScheduledAction> in, out;
	in << create(R"({"name":"Night","queue":"{11111111-1111-1111-1111-111111111111}","action":"speedLimit",
		"speedLimit":{"down":51200,"up":10240},"time":"23:30","days":[true,false,true,false,true,false,true]})");
	in[0].id = ScheduledAction::newId();

	ScheduledAction::saveAll(in);
	ScheduledAction::loadAll(out);

	QCOMPARE(out.size(), 1);
	QCOMPARE(SchedulerJson::actionToJson(out[0]), SchedulerJson::actionToJson(in[0]));
}

void TestSchedulerResources::createRepeated()
{
	ScheduledAction a = create(R"({"name":" Wake ","queue":"{11111111-1111-1111-1111-111111111111}","action":"resume","time":"07:05"})");
	QCOMPARE(a.name, QString("Wake"));
	QCOMPARE(a.action, ScheduledAction::ActionResumeAll);
	QVERIFY(a.repeated);
	QCOMPARE(a.whenRepeated, QTime(7, 5));
	for (bool d : a.daysRepeated)
		QVERIFY(d);

	const QJsonObject o = SchedulerJson::actionToJson(a);
	QCOMPARE(o["time"].toString(), QString("07:05"));
	QCOMPARE(o["queue"].toString(), Q1);
	QCOMPARE(o["days"].toArray().size(), 7);
	QVERIFY(!o.contains("speedLimit"));
}

void TestSchedulerResources::createSpeedLimit()
{
	ScheduledAction a = create(R"({"name":"Slow","queue":"{11111111-1111-1111-1111-111111111111}","action":"speedLimit",
		"speedLimit":{"down":0,"up":2048},"time":"08:00"})");
	QCOMPARE(a.action, ScheduledAction::ActionSetSpeedLimit);

	// the Scheduler reads the argument as [down, up] in bytes/s
	const QList<QVariant> sp = a.actionArgument.toList();
	QCOMPARE(sp.size(), 2);
	QCOMPARE(sp[0].toInt(), 0);
	QCOMPARE(sp[1].toInt(), 2048);

	const QJsonObject o = SchedulerJson::actionToJson(a);
	QCOMPARE(o["speedLimit"].toObject()["up"].toInt(), 2048);
}

void TestSchedulerResources::createOneTime()
{
	ScheduledAction a = create(R"({"name":"Once","queue":"{11111111-1111-1111-1111-111111111111}","action":"stop",
		"repeated":false,"at":"2026-10-20T03:00"})");
	QVERIFY(!a.repeated);
	QCOMPARE(a.whenOneTime, QDateTime(QDate(2026, 10, 20), QTime(3, 0)));
	QCOMPARE(SchedulerJson::actionToJson(a)["at"].toString(), QString("2026-10-20T03:00"));
}

void TestSchedulerResources::createValidation()
{
	const char* const q = R"("queue":"{11111111-1111-1111-1111-111111111111}")";
	auto fails = [&](const QString& json) {
		return failure([&] {
			ScheduledAction a;
			SchedulerJson::applyAction(a, QJsonDocument::fromJson(json.toUtf8()).object(), queues, true);
		});
	};

	QCOMPARE(fails(R"({})"), 422);
	QCOMPARE(fails(QString(R"({"name":"x",%1,"action":"resume"})").arg(q)), 422); // no time
	QCOMPARE(fails(QString(R"({"name":" ",%1,"action":"resume","time":"01:00"})").arg(q)), 422);
	QCOMPARE(fails(R"({"name":"x","queue":"{nope}","action":"resume","time":"01:00"})"), 422);
	QCOMPARE(fails(QString(R"({"name":"x",%1,"action":"explode","time":"01:00"})").arg(q)), 422);
	QCOMPARE(fails(QString(R"({"name":"x",%1,"action":"resume","time":"25:00"})").arg(q)), 422);
	QCOMPARE(fails(QString(R"({"name":"x",%1,"action":"resume","time":"1:00"})").arg(q)), 422);
	QCOMPARE(fails(QString(R"({"name":"x",%1,"action":"resume","time":"01:00","days":[true]})").arg(q)), 422);
	QCOMPARE(fails(QString(R"({"name":"x",%1,"action":"resume","time":"01:00","days":[false,false,false,false,false,false,false]})").arg(q)), 422);
	QCOMPARE(fails(QString(R"({"name":"x",%1,"action":"speedLimit","time":"01:00"})").arg(q)), 422);
	QCOMPARE(fails(QString(R"({"name":"x",%1,"action":"speedLimit","time":"01:00","speedLimit":{"down":-1,"up":0}})").arg(q)), 422);
	QCOMPARE(fails(QString(R"({"name":"x",%1,"action":"resume","repeated":false})").arg(q)), 422); // no at
	QCOMPARE(fails(QString(R"({"name":"x",%1,"action":"resume","repeated":false,"at":"2026-10-20"})").arg(q)), 422);
	QCOMPARE(fails(QString(R"({"name":"x",%1,"action":"resume","time":"01:00","id":"y"})").arg(q)), 422);
	QCOMPARE(fails(QString(R"({"name":"x",%1,"action":"resume","time":"01:00"})").arg(q)), 0);
}

void TestSchedulerResources::patchIsPartial()
{
	ScheduledAction a = create(R"({"name":"A","queue":"{11111111-1111-1111-1111-111111111111}","action":"resume","time":"07:05",
		"days":[true,true,true,true,true,false,false]})");
	a.id = "x";

	SchedulerJson::applyAction(a, obj(R"({"name":"B"})"), queues, false);
	QCOMPARE(a.name, QString("B"));
	QCOMPARE(a.id, QString("x"));
	QCOMPARE(a.whenRepeated, QTime(7, 5));
	QVERIFY(!a.daysRepeated[5]);

	// nothing is applied if any part is invalid
	QCOMPARE(failure([&] { SchedulerJson::applyAction(a, obj(R"({"name":"C","time":"bad"})"), queues, false); }), 422);
	QCOMPARE(a.name, QString("B"));
}

void TestSchedulerResources::patchSwitchesActionAndSchedule()
{
	ScheduledAction a = create(R"({"name":"A","queue":"{11111111-1111-1111-1111-111111111111}","action":"speedLimit",
		"speedLimit":{"down":1,"up":2},"time":"07:05"})");

	// the limits are kept while the action stays a speed limit
	SchedulerJson::applyAction(a, obj(R"({"time":"09:00"})"), queues, false);
	QCOMPARE(a.actionArgument.toList()[1].toInt(), 2);

	SchedulerJson::applyAction(a, obj(R"({"action":"stop"})"), queues, false);
	QVERIFY(!a.actionArgument.isValid());
	QVERIFY(!SchedulerJson::actionToJson(a).contains("speedLimit"));
	// switching back needs new limits
	QCOMPARE(failure([&] { SchedulerJson::applyAction(a, obj(R"({"action":"speedLimit"})"), queues, false); }), 422);

	QCOMPARE(failure([&] { SchedulerJson::applyAction(a, obj(R"({"repeated":false})"), queues, false); }), 0);
	QVERIFY(!a.repeated);
}

void TestSchedulerResources::oneTimeRunsOnce()
{
	ScheduledAction a = create(R"({"name":"A","queue":"{11111111-1111-1111-1111-111111111111}","action":"stop",
		"repeated":false,"at":"2026-10-20T03:00"})");
	const QDateTime t(QDate(2026, 10, 20), QTime(3, 0, 42));

	QVERIFY(a.dueAt(t));
	QVERIFY(!a.dueAt(t.addSecs(60)));
	QVERIFY(!a.dueAt(t.addDays(1)));
	a.done = true;
	QVERIFY(!a.dueAt(t));

	ScheduledAction r = create(R"({"name":"R","queue":"{11111111-1111-1111-1111-111111111111}","action":"stop",
		"time":"03:00","days":[true,false,false,false,false,false,false]})");
	QVERIFY(!r.dueAt(t)); // a Tuesday
	QVERIFY(r.dueAt(QDateTime(QDate(2026, 10, 19), QTime(3, 0, 1))));
}

void TestSchedulerResources::doneIsPersistedAndRearmed()
{
	QList<ScheduledAction> in, out;
	in << create(R"({"name":"A","queue":"{11111111-1111-1111-1111-111111111111}","action":"stop",
		"repeated":false,"at":"2026-10-20T03:00"})");
	in[0].id = ScheduledAction::newId();
	ScheduledAction::saveAll(in);

	ScheduledAction::markDone(in[0].id);
	ScheduledAction::loadAll(out);
	QVERIFY(out[0].done);
	QCOMPARE(SchedulerJson::actionToJson(out[0])["done"].toBool(), true);

	// rescheduling arms it again, an explicit "done" wins
	SchedulerJson::applyAction(out[0], obj(R"({"at":"2026-10-21T03:00"})"), queues, false);
	QVERIFY(!out[0].done);
	SchedulerJson::applyAction(out[0], obj(R"({"at":"2026-10-21T04:00","done":true})"), queues, false);
	QVERIFY(out[0].done);
	SchedulerJson::applyAction(out[0], obj(R"({"name":"B"})"), queues, false);
	QVERIFY(out[0].done);
}

void TestSchedulerResources::everyMinuteIsHandledOnce()
{
	const QDate d(2026, 10, 20);
	auto at = [&](int h, int m, int s) { return QDateTime(d, QTime(h, m, s)); };

	// same minute: nothing to do (no double firing)
	QVERIFY(ScheduledAction::minutesToCheck(at(3, 0, 5), at(3, 0, 55)).isEmpty());
	// the next minute
	QCOMPARE(ScheduledAction::minutesToCheck(at(3, 0, 55), at(3, 1, 5)), QList<QDateTime>() << at(3, 1, 0));
	// a late timer must not skip a minute
	QCOMPARE(ScheduledAction::minutesToCheck(at(3, 0, 50), at(3, 2, 10)),
		 QList<QDateTime>() << at(3, 1, 0) << at(3, 2, 0));
	// a long gap (suspend) is cut short
	QCOMPARE(ScheduledAction::minutesToCheck(at(1, 0, 0), at(3, 0, 0)).size(), 5);
	// the clock going backwards
	QVERIFY(ScheduledAction::minutesToCheck(at(3, 5, 0), at(3, 0, 0)).isEmpty());
	// across midnight
	QCOMPARE(ScheduledAction::minutesToCheck(at(23, 59, 30), QDateTime(d.addDays(1), QTime(0, 0, 10))),
		 QList<QDateTime>() << QDateTime(d.addDays(1), QTime(0, 0, 0)));
}

void TestSchedulerResources::removeForQueueDropsOnlyThatQueue()
{
	const QString Q2 = "{22222222-2222-2222-2222-222222222222}";
	QList<ScheduledAction> in, out;
	in << create(R"({"name":"A","queue":"{11111111-1111-1111-1111-111111111111}","action":"stop","time":"01:00"})");
	in << create(R"({"name":"B","queue":"{11111111-1111-1111-1111-111111111111}","action":"stop","time":"02:00"})");
	in << create(R"({"name":"C","queue":"{11111111-1111-1111-1111-111111111111}","action":"stop","time":"03:00"})");
	in[2].queue = QUuid(Q2);
	for (ScheduledAction& a : in)
		a.id = ScheduledAction::newId();
	ScheduledAction::saveAll(in);

	QVERIFY(ScheduledAction::removeForQueue(QUuid(Q1)));
	ScheduledAction::loadAll(out);
	QCOMPARE(out.size(), 1);
	QCOMPARE(out[0].name, QString("C"));
	QVERIFY(!ScheduledAction::removeForQueue(QUuid(Q1)));
}

QTEST_APPLESS_MAIN(TestSchedulerResources)
#include "tst_scheduleresources.moc"
