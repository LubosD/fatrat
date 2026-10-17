#include <QJsonDocument>
#include <QTest>

#include "plugins/Task.h"
#include "plugins/TaskManager.h"
#include "remote/TaskPush.h"

/* What the web UI is told about tasks, and how often. */
class tst_taskpush : public QObject
{
	Q_OBJECT
private slots:
	void init();
	void cleanup();
	void sendsAddedChangedRemoved();
	void holdsBackRapidProgress();
	void sendsOtherChangesAtOnce();
private:
	QList<QJsonObject> m_sent;
	TaskPush* m_push = nullptr;
	QList<QJsonObject> about(const QString& id) const;
};

void tst_taskpush::init()
{
	m_sent.clear();
	m_push = new TaskPush([this](const QJsonObject& m) { m_sent << m; });
}

void tst_taskpush::cleanup()
{
	delete m_push;
	TaskManager::instance()->setRetention(TaskManager::DefaultRetentionMs);
}

QList<QJsonObject> tst_taskpush::about(const QString& id) const
{
	QList<QJsonObject> rv;

	for (const QJsonObject& m : m_sent)
	{
		if (m.value("task").toObject().value("id").toString() == id || m.value("id").toString() == id)
			rv << m;
	}
	return rv;
}

void tst_taskpush::sendsAddedChangedRemoved()
{
	TaskManager::instance()->setRetention(10);
	Task* task = TaskManager::instance()->create("p.so", "Pushed");
	QString id = task->id();

	QCOMPARE(about(id).size(), 1);
	QCOMPARE(about(id)[0].value("type").toString(), QString("task"));
	QCOMPARE(about(id)[0].value("task").toObject().value("title").toString(), QString("Pushed"));

	task->finish(true);
	QCOMPARE(about(id).last().value("task").toObject().value("state").toString(), QString("done"));

	QTRY_COMPARE(about(id).last().value("type").toString(), QString("task-removed"));
}

void tst_taskpush::holdsBackRapidProgress()
{
	Task* task = TaskManager::instance()->create("p.so", "Busy");
	QString id = task->id();

	for (int i = 1; i <= 10; i++)
	{
		task->setProgress(i * 10);
		task->setStatus(QString("Step %1").arg(i));
	}

	// Only the creation went out at once; the rest is coalesced into one later message.
	QCOMPARE(about(id).size(), 1);
	QTRY_COMPARE(about(id).size(), 2);
	QCOMPARE(about(id).last().value("task").toObject().value("progress").toInt(), 100);
	QCOMPARE(about(id).last().value("task").toObject().value("status").toString(), QString("Step 10"));

	QTest::qWait(TaskPush::ProgressIntervalMs * 2);
	QCOMPARE(about(id).size(), 2);
}

void tst_taskpush::sendsOtherChangesAtOnce()
{
	Task* task = TaskManager::instance()->create("p.so", "Asks");
	QString id = task->id();

	task->setProgress(5); // held back
	QCOMPARE(about(id).size(), 1);

	task->ask(QJsonDocument::fromJson(R"({ "fields": [ { "type": "text", "key": "a" } ] })").object(), 60000,
		[](std::optional<QJsonObject>) {});

	// The question goes out at once, carrying the held-back progress with it.
	QCOMPARE(about(id).size(), 2);
	QJsonObject last = about(id).last().value("task").toObject();
	QCOMPARE(last.value("state").toString(), QString("waiting"));
	QCOMPARE(last.value("progress").toInt(), 5);
}

QTEST_GUILESS_MAIN(tst_taskpush)
#include "tst_taskpush.moc"
