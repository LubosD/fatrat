#include "Models.h"
#include "Store.h"
#include <QtTest>

using namespace tui;

static TransferInfo mk(const QString& id, const QString& name, TransferInfo::State s = TransferInfo::Active)
{
	TransferInfo t;
	t.uuid = id;
	t.name = name;
	t.state = s;
	return t;
}

static QueueInfo mkq(const QString& id)
{
	QueueInfo q;
	q.uuid = id;
	q.name = id;
	return q;
}

class TestTuiStore : public QObject
{
	Q_OBJECT
private slots:
	void transferInfoParsesThePropertiesFields()
	{
		const TransferInfo seeding = TransferInfo::fromJson(QJsonObject{
			{ "uuid", "{1}" }, { "mode", "Upload" }, { "primaryMode", "Download" }, { "completionCommand", "notify %N" } });
		QCOMPARE(seeding.completionCommand, QString("notify %N"));
		QVERIFY(seeding.upload);          // the current mode, for the list
		QVERIFY(!seeding.primaryUpload);  // what the transfer is: a download

		const TransferInfo upload = TransferInfo::fromJson(QJsonObject{ { "mode", "Upload" }, { "primaryMode", "Upload" } });
		QVERIFY(upload.primaryUpload);
		QCOMPARE(upload.completionCommand, QString());
	}

	void cursorFollowsTransferAcrossRefresh()
	{
		Store s;
		s.setQueues({ mkq("q") });
		s.setTransfers("q", { mk("a", "A"), mk("b", "B"), mk("c", "C") });
		s.setCursor(1);
		QCOMPARE(s.current()->uuid, QString("b"));
		// a new transfer is inserted before the cursor
		s.setTransfers("q", { mk("z", "Z"), mk("a", "A"), mk("b", "B"), mk("c", "C") });
		QCOMPARE(s.current()->uuid, QString("b"));
		// the current transfer disappears: the cursor stays in range
		s.setTransfers("q", { mk("z", "Z") });
		QCOMPARE(s.current()->uuid, QString("z"));
	}
	void staleQueueDataIsIgnored()
	{
		Store s;
		s.setQueues({ mkq("q1"), mkq("q2") });
		s.selectQueue(1);
		QVERIFY(!s.setTransfers("q1", { mk("a", "A") }));
		QVERIFY(s.transfers().isEmpty());
		QVERIFY(s.setTransfers("q2", { mk("b", "B") }));
	}
	void marksAndTargets()
	{
		Store s;
		s.setQueues({ mkq("q") });
		s.setTransfers("q", { mk("a", "A"), mk("b", "B"), mk("c", "C") });
		QCOMPARE(s.targets(), QStringList({ "a" }));
		s.toggleMark();            // marks a, moves to b
		s.toggleMark();            // marks b
		QCOMPARE(s.targets(), QStringList({ "a", "b" }));
		s.markAll();
		QCOMPARE(s.marks().size(), 3);
		s.markAll();               // second press unmarks
		QCOMPARE(s.marks().size(), 0);
		s.markRange(2, 0);
		QCOMPARE(s.targets(), QStringList({ "a", "b", "c" }));
		// marks of vanished transfers are dropped
		s.setTransfers("q", { mk("b", "B") });
		QCOMPARE(s.targets(), QStringList({ "b" }));
	}
	void filterKeepsCursorValid()
	{
		Store s;
		s.setQueues({ mkq("q") });
		s.setTransfers("q", { mk("a", "ubuntu.iso"), mk("b", "debian.iso"), mk("c", "ubuntu-server.iso") });
		s.setCursor(2);
		s.setFilter("UBUNTU");
		QCOMPARE(s.visible().size(), 2);
		QCOMPARE(s.current()->uuid, QString("c"));
		s.setFilter("nothing");
		QVERIFY(!s.current());
		QVERIFY(s.targets().isEmpty());
	}
	void hiddenMarkedTransfersAreNotTargets()
	{
		Store s;
		s.setQueues({ mkq("q") });
		s.setTransfers("q", { mk("a", "ubuntu.iso"), mk("b", "debian.iso") });
		s.markAll();
		QCOMPARE(s.targets().size(), 2);
		s.setFilter("ubuntu");
		QCOMPARE(s.targets(), QStringList({ "a" }));
	}
	void availabilityFollowsState()
	{
		Store s;
		s.setQueues({ mkq("q") });
		s.setTransfers("q", { mk("a", "A", TransferInfo::Active), mk("b", "B", TransferInfo::Paused) });
		Availability a = s.availability();
		QVERIFY(!a.resume);          // active: nothing to resume
		QVERIFY(a.pause);
		QVERIFY(!a.moveUp);          // first row
		QVERIFY(a.moveDown);
		s.setCursor(1);
		a = s.availability();
		QVERIFY(a.resume);
		QVERIFY(!a.pause);
		QVERIFY(a.moveUp);
		QVERIFY(!a.moveDown);
	}
	void otherQueuesBusy()
	{
		Store s;
		s.setQueues({ mkq("q1"), mkq("q2") });
		QVERIFY(!s.otherQueuesBusy());
		s.setQueueCount("q2", 3, 1);
		QVERIFY(s.otherQueuesBusy());
		s.setQueueCount("q2", 3, 0);
		QVERIFY(!s.otherQueuesBusy());
		s.setQueueCount("q1", 1, 1); // the selected queue is judged from its transfers
		QVERIFY(!s.otherQueuesBusy());
	}
	void queueSelectionSurvivesRefresh()
	{
		Store s;
		s.setQueues({ mkq("q1"), mkq("q2"), mkq("q3") });
		s.selectQueueByUuid("q2");
		s.setQueues({ mkq("q0"), mkq("q1"), mkq("q2"), mkq("q3") });
		QCOMPARE(s.currentQueueUuid(), QString("q2"));
		// the selected queue is deleted
		s.setQueues({ mkq("q0"), mkq("q1"), mkq("q3") });
		QVERIFY(!s.currentQueueUuid().isEmpty());
		QVERIFY(s.transfers().isEmpty());
	}
};

QTEST_APPLESS_MAIN(TestTuiStore)
#include "tst_tui_store.moc"
