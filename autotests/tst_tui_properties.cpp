#include "App.h"
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
