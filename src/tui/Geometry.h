#pragma once

namespace tui
{
	struct Rect
	{
		int x = 0, y = 0, w = 0, h = 0;
		bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
		bool empty() const { return w <= 0 || h <= 0; }
		int right() const { return x + w; }
		int bottom() const { return y + h; }
		Rect inset(int dx, int dy) const { return { x + dx, y + dy, w - 2 * dx, h - 2 * dy }; }
	};
}
