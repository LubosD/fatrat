#include "Actions.h"

namespace tui
{

#define C(ch) int(ch)
using A = Action;
using X = Context;

// Order matters: it is the order of the bottom bar
const QVector<Binding>& Keymap::bindings()
{
	static const QVector<Binding> b = {
		// ---- transfers pane
		{ X::Transfers, C('r'), false, A::Resume, "R", "Resume" },
		{ X::Transfers, C('f'), false, A::Force, "F", "Force" },
		{ X::Transfers, C('p'), false, A::Pause, "P", "Pause" },
		{ X::Transfers, C('n'), false, A::NewTransfer, "N", "New" },
		{ X::Transfers, C('d'), false, A::DeleteTransfers, "D", "Delete" },
		{ X::Transfers, C('e'), false, A::EditTransfer, "E", "Edit" },
		{ X::Transfers, C('m'), false, A::MoveMenu, "M", "Move" },
		{ X::Transfers, C('['), false, A::MoveUp, nullptr, nullptr },
		{ X::Transfers, C(']'), false, A::MoveDown, nullptr, nullptr },
		{ X::Transfers, C('s'), false, A::SendToQueue, "S", "Send" },
		{ X::Transfers, C('l'), false, A::LimitTransfer, "L", "Limit" },
		{ X::Transfers, C(' '), false, A::ToggleMark, "␣", "Mark" },
		{ X::Transfers, C('a'), false, A::MarkAll, nullptr, nullptr },
		{ X::Transfers, KeyEnter, false, A::Details, "⏎", "Details" },
		// ---- queues pane
		{ X::Queues, C('n'), false, A::NewQueue, "N", "New queue" },
		{ X::Queues, C('e'), false, A::EditQueue, "E", "Edit" },
		{ X::Queues, C('d'), false, A::DeleteQueue, "D", "Delete" },
		{ X::Queues, C('['), false, A::QueueMoveUp, "[", "Up" },
		{ X::Queues, C(']'), false, A::QueueMoveDown, "]", "Down" },
		{ X::Queues, C('l'), false, A::LimitQueue, "L", "Limit" },
		{ X::Queues, KeyEnter, false, A::OpenQueue, "⏎", "Open" },
		// ---- details view
		{ X::Details, KeyTab, false, A::NextTab, "Tab", "Next tab" },
		{ X::Details, KeyBackTab, false, A::PrevTab, nullptr, nullptr },
		{ X::Details, KeyEscape, false, A::Back, "Esc", "Back" },
		{ X::Details, C('b'), false, A::Back, nullptr, nullptr },
		{ X::Files, C('+'), false, A::PrioUp, "+", "Raise" },
		{ X::Files, C('='), false, A::PrioUp, nullptr, nullptr },
		{ X::Files, C('-'), false, A::PrioDown, "-", "Lower" },
		{ X::Files, C('s'), false, A::SkipFile, "S", "Skip" },
		{ X::Files, C('a'), false, A::SelectAllFiles, "A", "All" },
		{ X::Files, KeyEnter, false, A::ApplyFiles, "⏎", "Apply" },
		{ X::Files, KeyTab, false, A::NextTab, "Tab", "Next tab" },
		{ X::Files, KeyBackTab, false, A::PrevTab, nullptr, nullptr },
		{ X::Files, KeyEscape, false, A::Back, "Esc", "Back" },
		// ---- rss
		{ X::RssFeeds, C('n'), false, A::NewItem, "N", "New" },
		{ X::RssFeeds, C('e'), false, A::EditItem, "E", "Edit" },
		{ X::RssFeeds, C('d'), false, A::DeleteItem, "D", "Delete" },
		{ X::RssFeeds, C('r'), false, A::RefreshFeeds, "R", "Refresh now" },
		{ X::RssFeeds, KeyTab, false, A::FocusNext, "Tab", "Rules" },
		{ X::RssRules, C('n'), false, A::NewItem, "N", "New" },
		{ X::RssRules, C('e'), false, A::EditItem, "E", "Edit" },
		{ X::RssRules, C('d'), false, A::DeleteItem, "D", "Delete" },
		{ X::RssRules, C('t'), false, A::TestRegexp, "T", "Test" },
		{ X::RssRules, C('w'), false, A::DownloadedList, "W", "Downloaded" },
		{ X::RssRules, C('r'), false, A::RefreshFeeds, "R", "Refresh now" },
		{ X::RssRules, KeyTab, false, A::FocusNext, "Tab", "Feeds" },
		// ---- log
		{ X::Log, C('f'), false, A::Follow, "F", "Follow" },
		{ X::Log, C('/'), false, A::Search, "/", "Search" },
		{ X::Log, C('n'), false, A::SearchNext, nullptr, nullptr },
		{ X::Log, C('g'), false, A::TopOfLog, "G", "Top" },
		{ X::Log, KeyEnd, false, A::BottomOfLog, nullptr, nullptr },
		{ X::Log, C('t'), false, A::TransferLog, "T", "Transfer log" },
		// ---- global
		{ X::Global, C('1'), false, A::TabTransfers, nullptr, nullptr },
		{ X::Global, C('2'), false, A::TabRss, nullptr, nullptr },
		{ X::Global, C('3'), false, A::TabLog, nullptr, nullptr },
		{ X::Global, KeyF2, false, A::Settings, nullptr, nullptr },
		{ X::Global, KeyF3, false, A::GlobalLimit, nullptr, nullptr },
		{ X::Global, KeyTab, false, A::FocusNext, nullptr, nullptr },
		{ X::Global, KeyBackTab, false, A::FocusPrev, nullptr, nullptr },
		{ X::Global, KeyUp, false, A::Up, nullptr, nullptr },
		{ X::Global, C('k'), false, A::Up, nullptr, nullptr },
		{ X::Global, KeyDown, false, A::Down, nullptr, nullptr },
		{ X::Global, C('j'), false, A::Down, nullptr, nullptr },
		{ X::Global, KeyLeft, false, A::Left, nullptr, nullptr },
		{ X::Global, KeyRight, false, A::Right, nullptr, nullptr },
		{ X::Global, KeyPageUp, false, A::PageUp, nullptr, nullptr },
		{ X::Global, KeyPageDown, false, A::PageDown, nullptr, nullptr },
		{ X::Global, KeyHome, false, A::Home, nullptr, nullptr },
		{ X::Global, KeyEnd, false, A::End, nullptr, nullptr },
		{ X::Global, C('/'), false, A::Filter, "/", "Filter" },
		{ X::Global, KeyF1, false, A::Help, nullptr, nullptr },
		{ X::Global, C('?'), false, A::Help, "?", "Help" },
		{ X::Global, C('q'), false, A::Quit, "Q", "Quit" },
		{ X::Global, C('o'), false, A::Settings, nullptr, nullptr },
		{ X::Global, C('l'), true, A::Redraw, nullptr, nullptr },
		{ X::Global, KeyEscape, false, A::Back, nullptr, nullptr },
	};
	return b;
}

Action Keymap::lookup(Context ctx, const Event& e)
{
	if (e.type != Event::Key || e.alt)
		return Action::None;
	int key = e.key;
	if (key < 0x200000)
		key = QChar::toLower(char32_t(key));

	for (Context c : { ctx, Context::Global })
		for (const Binding& b : bindings())
			if (b.context == c && b.key == key && b.ctrl == e.ctrl)
				return b.action;
	return Action::None;
}

QVector<BarItem> Keymap::bar(Context ctx)
{
	QVector<BarItem> out;
	auto collect = [&](Context c) {
		for (const Binding& b : bindings())
			if (b.context == c && b.keyLabel)
				out.push_back({ QString::fromUtf8(b.keyLabel), QString::fromUtf8(b.label), b.action, true });
	};
	collect(ctx);
	if (ctx != Context::Global)
	{
		// keep the global entries (filter, help, quit) at the end, except those the context overrides
		QVector<BarItem> globals;
		for (const Binding& b : bindings())
			if (b.context == Context::Global && b.keyLabel)
			{
				bool dup = false;
				for (const BarItem& i : out)
					dup |= i.action == b.action;
				if (!dup && !(ctx == Context::Log && b.action == Action::Filter))
					globals.push_back({ QString::fromUtf8(b.keyLabel), QString::fromUtf8(b.label), b.action, true });
			}
		out += globals;
	}
	return out;
}

QString Keymap::keyFor(Context ctx, Action a)
{
	for (Context c : { ctx, Context::Global })
		for (const Binding& b : bindings())
			if (b.context == c && b.action == a)
			{
				if (b.keyLabel)
					return QString::fromUtf8(b.keyLabel);
				if (b.key < 0x200000)
					return QString(QChar(b.key)).toUpper();
			}
	return QString();
}

QString Keymap::eventToText(const Event& e)
{
	if (e.type != Event::Key)
		return QString();
	if (e.key < 0x200000)
		return (e.ctrl ? "Ctrl-" : "") + QString(QChar(e.key));
	return QString();
}

void HitMap::add(const Rect& r, Action a, int tag, int index, const QString& text)
{
	if (r.empty())
		return;
	m_hits.push_back({ r, a, tag, index, text });
}

const Hit* HitMap::at(int x, int y) const
{
	for (int i = m_hits.size() - 1; i >= 0; i--)
		if (m_hits[i].rect.contains(x, y))
			return &m_hits[i];
	return nullptr;
}

void HitMap::block(const Rect& screen)
{
	m_hits.push_back({ screen, Action::None, -1, 0, QString() });
}

}
