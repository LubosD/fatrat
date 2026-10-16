#pragma once
#include <QtGlobal>

namespace tui
{
	// Cursor and scroll position of a list
	struct ListNav
	{
		int cursor = 0, scroll = 0;

		void clamp(int count, int rows)
		{
			cursor = qBound(0, cursor, qMax(0, count - 1));
			if (cursor < scroll)
				scroll = cursor;
			if (rows > 0 && cursor >= scroll + rows)
				scroll = cursor - rows + 1;
			scroll = qBound(0, scroll, qMax(0, count - qMax(rows, 1)));
		}
		void move(int delta) { cursor += delta; }
		void scrollBy(int delta, int count, int rows)
		{
			scroll = qBound(0, scroll + delta, qMax(0, count - rows));
		}
	};
}
