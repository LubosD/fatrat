#pragma once
#include "Event.h"
#include "Geometry.h"
#include <QString>
#include <QVector>

namespace tui
{
	// Everything the user can do. Keys and mouse events are both translated into these.
	enum class Action
	{
		None,
		// global
		TabTransfers, TabRss, TabLog, Settings, Extensions, GlobalLimit, FocusNext, FocusPrev, Help, Quit, Redraw, Filter, Back, Confirm,
		// navigation
		Up, Down, PageUp, PageDown, Home, End, Left, Right,
		// transfers
		Resume, Force, Pause, NewTransfer, DeleteTransfers, EditTransfer, MoveMenu, SendToQueue, LimitTransfer,
		ToggleMark, MarkAll, Details, MoveUp, MoveDown, MoveTop, MoveBottom,
		// queues
		NewQueue, EditQueue, DeleteQueue, QueueMoveUp, QueueMoveDown, LimitQueue, OpenQueue,
		// details
		NextTab, PrevTab, PrioUp, PrioDown, SkipFile, SelectAllFiles, ApplyFiles,
		// rss
		NewItem, EditItem, DeleteItem, RefreshFeeds, TestRegexp, DownloadedList,
		// log
		Follow, Search, SearchNext, TopOfLog, BottomOfLog, TransferLog,
		// pointer
		ContextMenu, SelectRow, ActivateRow, ScrollUp, ScrollDown, FocusPane,
	};

	enum class Context
	{
		Global, Queues, Transfers, Details, Files, RssFeeds, RssRules, Log,
	};

	struct BarItem
	{
		QString key;       // as typed, e.g. "R" or "⏎"
		QString label;     // e.g. "Resume"
		Action action = Action::None;
		bool enabled = true;
	};

	struct Binding
	{
		Context context;
		int key;               // code point (lower case) or SpecialKey
		bool ctrl;
		Action action;
		const char* keyLabel;  // shown in the bar; null = not shown
		const char* label;
	};

	class Keymap
	{
	public:
		static const QVector<Binding>& bindings();
		// Context-specific bindings first, then global ones
		static Action lookup(Context ctx, const Event& e);
		static QVector<BarItem> bar(Context ctx);
		// The key that triggers an action in a context, as text (for menus)
		static QString keyFor(Context ctx, Action a);
		static QString eventToText(const Event& e);
	};

	// Rebuilt on every redraw: rectangles of everything that reacts to the mouse
	struct Hit
	{
		Rect rect;
		Action action = Action::None;
		int tag = 0;        // widget specific, e.g. which pane
		int index = 0;      // row index etc.
		QString text;
	};

	class HitMap
	{
	public:
		void clear() { m_hits.clear(); }
		void add(const Rect& r, Action a, int tag = 0, int index = 0, const QString& text = QString());
		// The topmost (last added) entry under the point
		const Hit* at(int x, int y) const;
		int size() const { return m_hits.size(); }
		const QVector<Hit>& hits() const { return m_hits; }
		// Covers everything drawn so far, for modal dialogs
		void block(const Rect& screen);
	private:
		QVector<Hit> m_hits;
	};
}
