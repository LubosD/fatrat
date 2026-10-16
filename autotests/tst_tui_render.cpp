#include "App.h"
#include "Dialogs.h"
#include "Format.h"
#include <QtTest>

using namespace tui;

static QueueInfo queue(const QString& id, const QString& name)
{
	QueueInfo q;
	q.uuid = id;
	q.name = name;
	q.defaultDirectory = "/downloads";
	return q;
}

static TransferInfo transfer(const QString& id, const QString& name, TransferInfo::State s, qint64 total = 1000, qint64 done = 0)
{
	TransferInfo t;
	t.uuid = id;
	t.name = name;
	t.state = s;
	t.total = total;
	t.done = done;
	t.speeds.down = s == TransferInfo::Active ? 1024 * 1024 : 0;
	return t;
}

class TestTuiRender : public QObject
{
	Q_OBJECT

	HttpClient* m_http = nullptr;
	ApiClient* m_api = nullptr;
	App* m_app = nullptr;

	void fill()
	{
		m_app->store().setQueues({ queue("q1", "Main queue"), queue("q2", "Movies") });
		m_app->store().setTransfers("q1", { transfer("a", "ubuntu-24.04.iso", TransferInfo::Active, 4000, 1000), transfer("b", "debian.iso", TransferInfo::Paused),
			transfer("c", "fedora.iso", TransferInfo::Completed, 1000, 1000), transfer("d", "broken.zip", TransferInfo::Failed) });
	}

	QString render(int w, int h, TextCanvas** out = nullptr)
	{
		m_canvas.reset(new TextCanvas(w, h));
		m_app->draw(*m_canvas);
		if (out)
			*out = m_canvas.get();
		return m_canvas->toText();
	}

	// Position of a hit with the given tag and action
	bool findHit(int tag, Action a, int& x, int& y, int index = -1)
	{
		for (const Hit& h : m_app->hits().hits())
			if (h.tag == tag && (a == Action::None || h.action == a) && (index < 0 || h.index == index))
			{
				x = h.rect.x + h.rect.w / 2;
				y = h.rect.y;
				return true;
			}
		return false;
	}

	std::unique_ptr<TextCanvas> m_canvas;

private slots:
	void init()
	{
		Glyphs::setAscii(false);
		m_http = new HttpClient;
		Endpoint ep;
		ep.kind = Endpoint::Unix;
		ep.socketPath = "/nonexistent/fatrat-test.sock";
		m_http->setEndpoint(ep);
		m_api = new ApiClient(m_http);
		m_app = new App(m_api);
		m_app->setPaused(true);
		fill();
	}
	void cleanup()
	{
		delete m_app;
		delete m_api;
		delete m_http;
	}

	void mainScreenShowsEverything()
	{
		const QString t = render(110, 34);
		QVERIFY(t.contains("FatRat"));
		QVERIFY(t.contains("Queues"));
		QVERIFY(t.contains("Main queue"));
		QVERIFY(t.contains("ubuntu-24.04.iso"));
		QVERIFY(t.contains("Paused"));
		QVERIFY(t.contains("Completed"));
		QVERIFY(t.contains("25%"));
		QVERIFY(t.contains("1.0 MB/s"));
		// the key bar of the focused pane
		QVERIFY(t.contains("Resume"));
		QVERIFY(t.contains("Quit"));
		QVERIFY(t.contains("Details"));
	}

	void noLineIsWiderThanTheScreen()
	{
		for (QSize sz : { QSize(60, 16), QSize(80, 24), QSize(99, 30), QSize(100, 30), QSize(200, 50) })
		{
			TextCanvas* c;
			render(sz.width(), sz.height(), &c);
			for (int y = 0; y < sz.height(); y++)
				QCOMPARE(displayWidth(c->line(y)), sz.width());
		}
	}

	void compactLayoutBelow100Columns()
	{
		const QString wide = render(110, 34), compact = render(90, 30);
		QVERIFY(wide.contains("Queues"));
		QVERIFY(!compact.contains("Queues"));
		QVERIFY(compact.contains("Queue "));
		QVERIFY(compact.contains("Main queue"));
		// the switcher arrows work
		int x, y;
		QVERIFY(findHit(TagQueueSwitch, Action::Right, x, y));
		m_app->handle(Event::mouseEvent(Event::Press, x, y));
		QCOMPARE(m_app->store().currentQueueUuid(), QString("q2"));
	}

	void tooSmallTerminal()
	{
		QVERIFY(render(50, 12).contains("too small"));
	}

	void asciiModeHasNoNonAscii()
	{
		Glyphs::setAscii(true);
		const QString t = render(110, 34);
		for (const QChar c : t)
			QVERIFY2(c.unicode() < 0x80 || c == '\n', qPrintable(QString("non-ASCII character U+%1").arg(int(c.unicode()), 4, 16)));
		QVERIFY(t.contains("Resume"));
		Glyphs::setAscii(false);
	}

	void keyboardNavigationAndMarks()
	{
		render(110, 34);
		m_app->handle(Event::charEvent('j'));
		QCOMPARE(m_app->store().current()->uuid, QString("b"));
		m_app->handle(Event::charEvent(' '));
		m_app->handle(Event::charEvent(' '));
		QCOMPARE(m_app->store().targets(), QStringList({ "b", "c" }));
		QVERIFY(render(110, 34).contains("2 marked"));
		m_app->handle(Event::keyEvent(KeyEscape));
		QVERIFY(m_app->store().marks().isEmpty());
	}

	void focusSwitchChangesTheKeyBar()
	{
		QVERIFY(render(110, 34).contains("Resume"));
		m_app->handle(Event::keyEvent(KeyLeft));
		const QString t = render(110, 34);
		QVERIFY(t.contains("New queue"));
		QVERIFY(!t.contains("Resume"));
		// d on the queue pane is queue deletion, not transfer deletion
		m_app->handle(Event::charEvent('d'));
		QVERIFY(m_app->hasDialog());
		QVERIFY(render(110, 34).contains("Delete queue"));
	}

	void clickingTheKeyBarRunsTheAction()
	{
		render(110, 34);
		m_app->handle(Event::charEvent('j')); // debian.iso, paused: Resume is possible
		render(110, 34);
		int x, y;
		QVERIFY(findHit(TagBar, Action::Resume, x, y));
		m_app->handle(Event::mouseEvent(Event::Press, x, y));
		QCOMPARE(m_app->store().current()->state, TransferInfo::Waiting); // set locally at once
	}

	void clickingTabsSwitchesViews()
	{
		render(110, 34);
		int x, y;
		QVERIFY(findHit(TagTab, Action::TabRss, x, y));
		m_app->handle(Event::mouseEvent(Event::Press, x, y));
		QCOMPARE(m_app->tab(), App::Tab::Rss);
		QVERIFY(render(110, 34).contains("Feeds"));
		QVERIFY(findHit(TagTab, Action::TabLog, x, y));
		m_app->handle(Event::mouseEvent(Event::Press, x, y));
		QCOMPARE(m_app->tab(), App::Tab::Log);
	}

	void mouseSelectionMarksAndMenu()
	{
		render(110, 34);
		int x, y;
		QVERIFY(findHit(TagTransferRow, Action::None, x, y, 2));
		m_app->handle(Event::mouseEvent(Event::Press, x, y));
		m_app->handle(Event::mouseEvent(Event::Release, x, y));
		QCOMPARE(m_app->store().current()->uuid, QString("c"));

		Event ctrl = Event::mouseEvent(Event::Press, x, y - 2);
		ctrl.ctrl = true;
		m_app->handle(ctrl);
		QVERIFY(m_app->store().marks().contains("a"));

		Event right = Event::mouseEvent(Event::Press, x, y, 3);
		m_app->handle(right);
		QVERIFY(m_app->hasDialog());
		const QString t = render(110, 34);
		QVERIFY(t.contains("Force resume"));
		QVERIFY(t.contains("Send to queue"));
		m_app->handle(Event::keyEvent(KeyEscape));
		QVERIFY(!m_app->hasDialog());
	}

	void wheelScrollsWithoutMovingTheCursor()
	{
		QVector<TransferInfo> many;
		for (int i = 0; i < 60; i++)
			many << transfer(QString("t%1").arg(i), QString("file-%1.iso").arg(i), TransferInfo::Paused);
		m_app->store().setTransfers("q1", many);
		render(110, 30);
		QVERIFY(!m_canvas->toText().contains("file-40.iso"));
		for (int i = 0; i < 12; i++)
			m_app->handle(Event::mouseEvent(Event::WheelDown, 40, 8));
		const QString t = render(110, 30);
		QVERIFY(t.contains("file-40.iso"));
		QCOMPARE(m_app->store().current()->uuid, QString("t0"));
		// keyboard movement brings the cursor back into view
		m_app->handle(Event::charEvent('k'));
		QVERIFY(render(110, 30).contains("file-0.iso"));
	}

	void dialogsBlockTheViewBelow()
	{
		m_app->handle(Event::charEvent('?'));
		QVERIFY(m_app->hasDialog());
		render(110, 34);
		int x, y;
		// the tabs are covered by the dialog's blocker
		QVERIFY(m_app->hits().at(5, 1) && m_app->hits().at(5, 1)->tag == -1);
		Q_UNUSED(x); Q_UNUSED(y);
		m_app->handle(Event::keyEvent(KeyEscape));
		QVERIFY(!m_app->hasDialog());
	}

	// ---- dialog widgets
	void formEditingAndSubmit()
	{
		QString submitted;
		FormDialog d("Test", 60);
		d.addText("a", "First", "abc");
		d.addText("b", "Second");
		d.addCheck("c", "Flag", false);
		d.addSelect("s", "Pick", { "One", "Two", "Three" }, { "1", "2", "3" }, 0);
		d.setSubmit("Go", [&](FormDialog& f) {
			if (f.text("b").isEmpty())
			{
				f.setError("need b");
				return false;
			}
			submitted = f.text("a") + "|" + f.text("b") + "|" + (f.checked("c") ? "x" : "-") + "|" + f.value("s");
			return true;
		});
		TextCanvas c(80, 24);
		HitMap hits;
		d.draw(c, hits, { 0, 0, 80, 24 });

		d.handle(Event::charEvent('X'));                 // starts in the first text field, cursor at the end
		d.handle(Event::keyEvent(KeyBackspace));
		d.handle(Event::keyEvent(KeyBackspace));
		d.handle(Event::keyEvent(KeyHome));
		d.handle(Event::charEvent('>'));
		QCOMPARE(d.text("a"), QString(">ab"));
		d.handle(Event::keyEvent(KeyEnter));             // b is empty: rejected
		QVERIFY(!d.finished());
		d.handle(Event::keyEvent(KeyTab));
		d.handle(Event::pasteEvent("pas\nted"));
		d.handle(Event::keyEvent(KeyTab));
		d.handle(Event::charEvent(' '));                 // toggles the check
		d.handle(Event::keyEvent(KeyTab));
		d.handle(Event::keyEvent(KeyRight));             // next option
		d.handle(Event::keyEvent(KeyRight));
		d.handle(Event::keyEvent(KeyEnter));
		QVERIFY(d.finished());
		QCOMPARE(submitted, QString(">ab|pas ted|x|3"));
	}

	void formMouse()
	{
		FormDialog d("Mouse", 60);
		d.addText("a", "Name", "hello");
		d.addCheck("c", "Flag", false);
		d.addSelect("s", "Pick", { "One", "Two" }, { "1", "2" }, 0);
		TextCanvas c(80, 24);
		HitMap hits;
		d.draw(c, hits, { 0, 0, 80, 24 });
		const Rect r = d.rect();

		// find rows by their labels
		auto rowOf = [&](const QString& label) {
			for (int y = 0; y < 24; y++)
				if (c.line(y).contains(label))
					return y;
			return -1;
		};
		const int checkRow = rowOf("Flag"), selRow = rowOf("Pick");
		const int vx = r.x + 2 + 5; // label column
		d.handle(Event::mouseEvent(Event::Press, vx, checkRow));
		QVERIFY(d.checked("c"));
		d.draw(c, hits, { 0, 0, 80, 24 });
		// the right arrow of the select
		int arrowX = -1;
		for (int x = vx; x < r.right(); x++)
			if (c.charAt(x, selRow) == QString::fromUtf8("▸"))
				arrowX = x;
		QVERIFY(arrowX > 0);
		d.handle(Event::mouseEvent(Event::Press, arrowX, selRow));
		QCOMPARE(d.value("s"), QString("2"));
		// a click outside cancels
		d.handle(Event::mouseEvent(Event::Press, 0, 0));
		QVERIFY(d.finished());
	}

	void choiceDialogLetters()
	{
		int result = -2;
		ChoiceDialog d("Delete", { "Sure?" }, { { "Y", "Yes", 0 }, { "D", "Delete with data", 1 }, { "N", "No", -1 } }, [&](int r) { result = r; });
		d.handle(Event::charEvent('x'));
		QCOMPARE(result, -2);
		d.handle(Event::charEvent('D'));
		QCOMPARE(result, 1);
		QVERIFY(d.finished());
	}

	void choiceDialogEscCancels()
	{
		int result = -2;
		ChoiceDialog d("Quit", { "Sure?" }, { { "Y", "Yes", 0 } }, [&](int r) { result = r; });
		d.handle(Event::keyEvent(KeyEscape));
		QCOMPARE(result, -1);
	}

	void menuSkipsDisabledAndSeparators()
	{
		QString chosen;
		QVector<MenuItem> items;
		items << MenuItem{ "Off", "O", false, false, [&]() { chosen = "off"; } } << MenuItem::sep()
			<< MenuItem{ "First", "F", true, false, [&]() { chosen = "first"; } }
			<< MenuItem{ "Second", "S", true, false, [&]() { chosen = "second"; } };
		MenuDialog d(items, 5, 5);
		TextCanvas c(80, 24);
		HitMap hits;
		d.draw(c, hits, { 0, 0, 80, 24 });
		d.handle(Event::keyEvent(KeyDown));
		d.handle(Event::keyEvent(KeyEnter));
		QCOMPARE(chosen, QString("second")); // the cursor started on First
		QVERIFY(d.finished());
	}

	void menuLetterAndDisabledLetter()
	{
		QString chosen;
		QVector<MenuItem> items = { MenuItem{ "Off", "O", false, false, [&]() { chosen = "off"; } }, MenuItem{ "Go", "G", true, false, [&]() { chosen = "go"; } } };
		MenuDialog d(items, 0, 0);
		d.handle(Event::charEvent('o'));
		QVERIFY(chosen.isEmpty());
		d.handle(Event::charEvent('g'));
		QCOMPARE(chosen, QString("go"));
	}

	void menuStaysOnScreen()
	{
		MenuDialog d({ MenuItem{ "A long entry here", "X", true, false, nullptr }, MenuItem{ "B", "", true, false, nullptr } }, 78, 23);
		TextCanvas c(80, 24);
		HitMap hits;
		d.draw(c, hits, { 0, 0, 80, 24 });
		QVERIFY(d.rect().right() <= 80);
		QVERIFY(d.rect().bottom() <= 24);
	}
};

QTEST_MAIN(TestTuiRender)
#include "tst_tui_render.moc"
