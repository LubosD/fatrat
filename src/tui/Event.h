#pragma once
#include <QString>

namespace tui
{
	// Special keys live above the Unicode range
	enum SpecialKey
	{
		KeyNone = 0,
		KeyUp = 0x200001, KeyDown, KeyLeft, KeyRight, KeyPageUp, KeyPageDown, KeyHome, KeyEnd,
		KeyEnter, KeyEscape, KeyTab, KeyBackTab, KeyBackspace, KeyDelete, KeyInsert,
		KeyF1, KeyF2, KeyF3, KeyF4, KeyF5, KeyF6, KeyF7, KeyF8, KeyF9, KeyF10, KeyF11, KeyF12,
	};

	struct Event
	{
		enum Type { None, Key, Mouse, Resize, Paste } type = None;

		// Key: a Unicode code point or a SpecialKey
		int key = 0;
		bool ctrl = false, alt = false, shift = false;

		enum MouseKind { Press, Release, Drag, WheelUp, WheelDown } mouse = Press;
		int button = 1;               // 1 left, 2 middle, 3 right
		int x = 0, y = 0;
		bool doubleClick = false;     // set on the Press of the second click

		QString text;                 // Paste

		static Event keyEvent(int k, bool ctrl = false) { Event e; e.type = Key; e.key = k; e.ctrl = ctrl; return e; }
		static Event charEvent(QChar c) { return keyEvent(c.unicode()); }
		static Event mouseEvent(MouseKind kind, int x, int y, int button = 1)
		{
			Event e; e.type = Mouse; e.mouse = kind; e.x = x; e.y = y; e.button = button; return e;
		}
		static Event pasteEvent(const QString& t) { Event e; e.type = Paste; e.text = t; return e; }

		bool isPrintable() const { return type == Key && !ctrl && !alt && key >= 0x20 && key < 0x200000 && key != 0x7f; }
		bool is(int k) const { return type == Key && key == k && !ctrl; }
		// Case-insensitive letter, without Ctrl
		bool isLetter(char c) const { return type == Key && !ctrl && !alt && key < 0x200000 && QChar(key).toLower() == QChar(c); }
	};
}
