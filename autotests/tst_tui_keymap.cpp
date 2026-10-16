#include "Actions.h"
#include <QtTest>

using namespace tui;

class TestTuiKeymap : public QObject
{
	Q_OBJECT
private slots:
	void contextWinsOverGlobal()
	{
		QCOMPARE(Keymap::lookup(Context::Transfers, Event::charEvent('r')), Action::Resume);
		QCOMPARE(Keymap::lookup(Context::Transfers, Event::charEvent('R')), Action::Resume);
		QCOMPARE(Keymap::lookup(Context::Queues, Event::charEvent('d')), Action::DeleteQueue);
		QCOMPARE(Keymap::lookup(Context::Transfers, Event::charEvent('d')), Action::DeleteTransfers);
		QCOMPARE(Keymap::lookup(Context::RssFeeds, Event::charEvent('r')), Action::RefreshFeeds);
	}
	void globalKeys()
	{
		QCOMPARE(Keymap::lookup(Context::Log, Event::charEvent('q')), Action::Quit);
		QCOMPARE(Keymap::lookup(Context::Transfers, Event::charEvent('j')), Action::Down);
		QCOMPARE(Keymap::lookup(Context::Transfers, Event::keyEvent(KeyUp)), Action::Up);
		QCOMPARE(Keymap::lookup(Context::Transfers, Event::charEvent('3')), Action::TabLog);
		QCOMPARE(Keymap::lookup(Context::Transfers, Event::keyEvent('l', true)), Action::Redraw);
		QCOMPARE(Keymap::lookup(Context::Transfers, Event::charEvent('@')), Action::None);
	}
	void everyBarItemIsReachableByItsKey()
	{
		for (Context c : { Context::Queues, Context::Transfers, Context::Details, Context::Files, Context::RssFeeds, Context::RssRules, Context::Log })
		{
			const auto bar = Keymap::bar(c);
			QVERIFY(!bar.isEmpty());
			for (const BarItem& i : bar)
				QVERIFY2(!Keymap::keyFor(c, i.action).isEmpty(), qPrintable(i.label));
		}
	}
	void barEndsWithGlobals()
	{
		const auto bar = Keymap::bar(Context::Transfers);
		QCOMPARE(bar.last().action, Action::Quit);
		bool hasHelp = false;
		for (const BarItem& i : bar)
			hasHelp |= i.action == Action::Help;
		QVERIFY(hasHelp);
	}
	void hitMapTopmostWins()
	{
		HitMap m;
		m.add({ 0, 0, 10, 10 }, Action::Resume);
		m.add({ 2, 2, 3, 3 }, Action::Pause);
		QCOMPARE(m.at(3, 3)->action, Action::Pause);
		QCOMPARE(m.at(8, 8)->action, Action::Resume);
		QVERIFY(!m.at(20, 20));
		m.block({ 0, 0, 80, 24 });
		QCOMPARE(m.at(3, 3)->tag, -1);
	}
};

QTEST_APPLESS_MAIN(TestTuiKeymap)
#include "tst_tui_keymap.moc"
