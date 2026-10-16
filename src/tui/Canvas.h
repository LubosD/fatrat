#pragma once
#include "Geometry.h"
#include <QString>
#include <QVector>

namespace tui
{
	enum class Style : unsigned char
	{
		Normal, Dim, Bold, Hl, Title, TabOn, Selected, SelectedDim, Header, Status, StatusWarn,
		KeyBar, KeyHint, Green, Yellow, Red, Cyan, Input, Mark, Border, BorderFocus, Error,
		MenuSel, Count
	};

	// Characters used for borders, bars and markers; swapped for ASCII lookalikes with --ascii
	struct Glyphs
	{
		QString h, v, tl, tr, bl, br, teeL, teeR, barFull, barEmpty, ellipsis, marked, cursor, bullet, radioOn, radioOff,
			checkOn, checkOff, up, down, inf, left, right, dot, scrollThumb, scrollTrack;
		QString spark[8];
		static const Glyphs& get();
		static void setAscii(bool ascii);
		static bool isAscii();
		// Replaces every non-ASCII character by a single-cell lookalike (only used in ASCII mode)
		static QString fold(const QString& s);
	};

	class Canvas
	{
	public:
		virtual ~Canvas() = default;
		virtual int width() const = 0;
		virtual int height() const = 0;
		// Writes text starting at (x, y), clipped to the canvas. Wide characters take two cells.
		virtual void put(int x, int y, const QString& text, Style style) = 0;
		virtual void setCursor(int x, int y, bool visible) = 0;

		void fill(const Rect& r, Style style, const QString& ch = " ");
		void putClipped(const Rect& clip, int x, int y, const QString& text, Style style);
		void box(const Rect& r, Style style, const QString& title = QString(), Style titleStyle = Style::Hl);
		void hline(int x, int y, int w, Style style);
		void vline(int x, int y, int h, Style style);
	};

	// In-memory canvas for tests and for dumping screens as text
	class TextCanvas : public Canvas
	{
	public:
		TextCanvas(int w, int h);
		int width() const override { return m_w; }
		int height() const override { return m_h; }
		void put(int x, int y, const QString& text, Style style) override;
		void setCursor(int x, int y, bool visible) override { m_cx = x; m_cy = y; m_cursor = visible; }

		QString line(int y) const;
		QString toText() const;
		Style styleAt(int x, int y) const;
		QString charAt(int x, int y) const;
		bool cursorVisible() const { return m_cursor; }
		int cursorX() const { return m_cx; }
		int cursorY() const { return m_cy; }
		void clear();

	private:
		struct Cell { QString ch = " "; Style style = Style::Normal; };
		int m_w, m_h, m_cx = 0, m_cy = 0;
		bool m_cursor = false;
		QVector<Cell> m_cells;
	};
}
