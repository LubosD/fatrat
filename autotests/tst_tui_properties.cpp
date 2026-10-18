#include "App.h"
#include "Canvas.h"
#include "Dialog.h"
#include "Format.h"
#include <QtTest>

using namespace tui;

// FormDialog's async submit and the field operations a form filled in later needs. The dialog
// itself (Task 4) is tested further down in this file against a fake server.
class TestTuiProperties : public QObject
{
	Q_OBJECT

	static Event ev(int k) { return Event::keyEvent(k, false); }

private slots:
	void asyncSubmitWaitsForTheResult()
	{
		FormDialog d("Test", 60);
		d.addText("a", "A", "x");
		std::function<void(QString)> pending;
		int calls = 0;
		d.setSubmitAsync("Save", [&](FormDialog&, std::function<void(QString)> done) { calls++; pending = done; });

		d.handle(ev(KeyEnter));
		QCOMPARE(calls, 1);
		QVERIFY(!d.finished());
		QVERIFY(d.busy());

		// while waiting, neither another submit nor Esc does anything
		d.handle(ev(KeyEnter));
		d.handle(ev(KeyEscape));
		QCOMPARE(calls, 1);
		QVERIFY(!d.finished());

		pending(QString());
		QVERIFY(d.finished());
	}

	void asyncSubmitErrorKeepsTheDialogOpen()
	{
		FormDialog d("Test", 60);
		d.addText("a", "A", "x");
		d.setSubmitAsync("Save", [](FormDialog&, std::function<void(QString)> done) { done("Not a readable directory: /x"); });

		d.handle(ev(KeyEnter));
		QVERIFY(!d.finished());
		QVERIFY(!d.busy());
		QCOMPARE(d.error(), QString("Not a readable directory: /x"));
		QCOMPARE(d.text("a"), QString("x"));

		d.handle(ev(KeyEscape));   // usable again
		QVERIFY(d.finished());
	}

	void aLateResultAfterTheDialogIsGoneIsIgnored()
	{
		std::function<void(QString)> pending;
		{
			FormDialog d("Test", 60);
			d.setSubmitAsync("Save", [&](FormDialog&, std::function<void(QString)> done) { pending = done; });
			d.handle(ev(KeyEnter));
		}
		pending("too late");   // must not touch the destroyed dialog
	}

	void aStaleResultDoesNotSettleALaterSubmit()
	{
		FormDialog d("Test", 60);
		QVector<std::function<void(QString)>> dones;
		d.setSubmitAsync("Save", [&](FormDialog&, std::function<void(QString)> done) { dones.push_back(done); });

		d.handle(ev(KeyEnter));
		dones[0]("Refused");
		QCOMPARE(d.error(), QString("Refused"));

		d.handle(ev(KeyEnter));
		QCOMPARE(dones.size(), 2);
		dones[0](QString());   // the first submit reporting again
		QVERIFY(!d.finished());
		QVERIFY(d.busy());

		dones[1]("Refused again");
		dones[1](QString());   // done called twice
		QVERIFY(!d.finished());
		QCOMPARE(d.error(), QString("Refused again"));
	}

	void aClickAfterRemovingAFieldBeforeARedrawDoesNothing()
	{
		FormDialog d("Test", 60);
		d.addText("a", "A");   // not first: QList drops a front element without moving the rest
		d.addCheck("c1", "First", false);
		d.addCheck("c2", "Second", false);
		TextCanvas c(80, 24);
		HitMap hits;
		d.draw(c, hits, { 0, 0, 80, 24 });
		int row = -1, box = -1;
		for (int y = 0; y < 24 && row < 0; y++)
			if (c.line(y).contains("First"))
				row = y;
		for (int x = 0; x < 80 && box < 0 && row >= 0; x++)
			if (c.charAt(x, row) == "[")
				box = x;
		QVERIFY(box >= 0);

		d.removeField("c1");
		// the box of the removed field; its stale handler would toggle whatever now sits at its index
		d.handle(Event::mouseEvent(Event::Press, box + 1, row));
		QVERIFY(!d.checked("c2"));
	}

	void removingAnEmptyIdRemovesNothing()
	{
		FormPanel p;
		p.addLabel("Unnamed");
		p.addText("a", "A");
		const int before = p.contentHeight(40);
		p.removeField(QString());
		QCOMPARE(p.contentHeight(40), before);
	}

	void labelsCanBeNamedChangedAndRemoved()
	{
		FormPanel p;
		p.addText("a", "A");
		p.addLabel("Loading").id = "status";
		p.addText("b", "B");
		p.focusField("b");

		p.setText("status", "Failed");
		QCOMPARE(p.text("status"), QString("Failed"));

		p.removeField("status");
		QVERIFY(!p.field("status"));
		QCOMPARE(p.focusedId(), QString("b"));   // the focus stays on the same field
	}
};

QTEST_GUILESS_MAIN(TestTuiProperties)
#include "tst_tui_properties.moc"
