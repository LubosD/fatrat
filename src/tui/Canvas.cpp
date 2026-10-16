#include "Canvas.h"
#include "Format.h"
#include <QHash>

namespace tui
{

static bool g_ascii = false;

static Glyphs makeUnicode()
{
	Glyphs g;
	g.h = "─"; g.v = "│"; g.tl = "┌"; g.tr = "┐"; g.bl = "└"; g.br = "┘"; g.teeL = "├"; g.teeR = "┤";
	g.barFull = "█"; g.barEmpty = "░"; g.ellipsis = "…"; g.marked = "●"; g.cursor = "▸"; g.bullet = "•";
	g.radioOn = "●"; g.radioOff = "○"; g.checkOn = "[x]"; g.checkOff = "[ ]"; g.up = "↑"; g.down = "↓"; g.inf = "∞";
	g.left = "◂"; g.right = "▸"; g.dot = "●"; g.scrollThumb = "█"; g.scrollTrack = "░";
	const char* sp[8] = { "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█" };
	for (int i = 0; i < 8; i++)
		g.spark[i] = QString::fromUtf8(sp[i]);
	return g;
}

static Glyphs makeAscii()
{
	Glyphs g;
	g.h = "-"; g.v = "|"; g.tl = "+"; g.tr = "+"; g.bl = "+"; g.br = "+"; g.teeL = "+"; g.teeR = "+";
	g.barFull = "#"; g.barEmpty = "."; g.ellipsis = "~"; g.marked = "*"; g.cursor = ">"; g.bullet = "*";
	g.radioOn = "(*)"; g.radioOff = "( )"; g.checkOn = "[x]"; g.checkOff = "[ ]"; g.up = "^"; g.down = "v"; g.inf = "-";
	g.left = "<"; g.right = ">"; g.dot = "*"; g.scrollThumb = "#"; g.scrollTrack = ":";
	const char* sp[8] = { "_", ".", "-", "~", "=", "+", "#", "#" };
	for (int i = 0; i < 8; i++)
		g.spark[i] = QString::fromLatin1(sp[i]);
	return g;
}

const Glyphs& Glyphs::get()
{
	static const Glyphs uni = makeUnicode();
	static const Glyphs asc = makeAscii();
	return g_ascii ? asc : uni;
}

void Glyphs::setAscii(bool ascii) { g_ascii = ascii; }

QString Glyphs::fold(const QString& s)
{
	bool plain = true;
	for (const QChar c : s)
		plain &= c.unicode() < 0x80;
	if (plain)
		return s;

	static const QHash<char32_t, char> map = {
		{ U'·', '-' }, { U'…', '~' }, { U'—', '-' }, { U'–', '-' }, { U'✓', '*' }, { U'⏎', '>' }, { U'␣', '_' }, { U'⌫', '<' },
		{ U'↑', '^' }, { U'↓', 'v' }, { U'←', '<' }, { U'→', '>' }, { U'●', '*' }, { U'○', 'o' }, { U'◂', '<' }, { U'▸', '>' },
		{ U'│', '|' }, { U'─', '-' }, { U'┌', '+' }, { U'┐', '+' }, { U'└', '+' }, { U'┘', '+' }, { U'├', '+' }, { U'┤', '+' },
		{ U'█', '#' }, { U'░', '.' }, { U'∞', '-' }, { U'▁', '_' }, { U'▂', '.' }, { U'▃', '-' }, { U'▄', '~' }, { U'▅', '=' },
		{ U'▆', '+' }, { U'▇', '#' },
	};
	QString out;
	for (char32_t c : s.toUcs4())
	{
		if (c < 0x80)
			out += QChar(char16_t(c));
		else if (map.contains(c))
			out += QChar(map[c]);
		else
			out += QString(displayWidth(QString::fromUcs4(&c, 1)) == 2 ? "??" : "?");
	}
	return out;
}
bool Glyphs::isAscii() { return g_ascii; }

void Canvas::fill(const Rect& r, Style style, const QString& ch)
{
	if (r.empty())
		return;
	const QString row = repeat(ch, r.w);
	for (int y = r.y; y < r.bottom(); y++)
		put(r.x, y, row, style);
}

void Canvas::putClipped(const Rect& clip, int x, int y, const QString& text, Style style)
{
	if (y < clip.y || y >= clip.bottom())
		return;
	// cut the text to the clip rectangle horizontally
	QString t = text;
	if (x < clip.x)
	{
		int skip = clip.x - x;
		QString out;
		int w = 0;
		for (char32_t c : t.toUcs4())
		{
			QString s = QString::fromUcs4(&c, 1);
			int cw = displayWidth(s);
			if (w >= skip)
				out += s;
			w += cw;
		}
		t = out;
		x = clip.x;
	}
	int avail = clip.right() - x;
	if (avail <= 0)
		return;
	put(x, y, fit(t, qMin(avail, displayWidth(t))), style);
}

void Canvas::hline(int x, int y, int w, Style style)
{
	put(x, y, repeat(Glyphs::get().h, w), style);
}

void Canvas::vline(int x, int y, int h, Style style)
{
	for (int i = 0; i < h; i++)
		put(x, y + i, Glyphs::get().v, style);
}

void Canvas::box(const Rect& r, Style style, const QString& title, Style titleStyle)
{
	if (r.w < 2 || r.h < 2)
		return;
	const Glyphs& g = Glyphs::get();
	put(r.x, r.y, g.tl + repeat(g.h, r.w - 2) + g.tr, style);
	for (int y = r.y + 1; y < r.bottom() - 1; y++)
	{
		put(r.x, y, g.v, style);
		put(r.right() - 1, y, g.v, style);
	}
	put(r.x, r.bottom() - 1, g.bl + repeat(g.h, r.w - 2) + g.br, style);
	if (!title.isEmpty() && r.w > 6)
		put(r.x + 2, r.y, " " + elide(title, r.w - 6, Glyphs::isAscii()) + " ", titleStyle);
}

// ------------------------------------------------------------- TextCanvas

TextCanvas::TextCanvas(int w, int h) : m_w(w), m_h(h), m_cells(w * h)
{
}

void TextCanvas::clear()
{
	m_cells = QVector<Cell>(m_w * m_h);
}

void TextCanvas::put(int x, int y, const QString& text, Style style)
{
	if (y < 0 || y >= m_h)
		return;
	for (char32_t c : (Glyphs::isAscii() ? Glyphs::fold(text) : text).toUcs4())
	{
		const QString s = QString::fromUcs4(&c, 1);
		int cw = displayWidth(s);
		if (cw == 0)
			continue;
		if (x >= 0 && x + cw <= m_w)
		{
			m_cells[y * m_w + x] = { s, style };
			if (cw == 2)
				m_cells[y * m_w + x + 1] = { QString(), style };
		}
		x += cw;
	}
}

QString TextCanvas::line(int y) const
{
	QString out;
	for (int x = 0; x < m_w; x++)
		out += m_cells[y * m_w + x].ch;
	return out;
}

QString TextCanvas::toText() const
{
	QStringList lines;
	for (int y = 0; y < m_h; y++)
		lines << line(y);
	return lines.join('\n');
}

Style TextCanvas::styleAt(int x, int y) const { return m_cells[y * m_w + x].style; }
QString TextCanvas::charAt(int x, int y) const { return m_cells[y * m_w + x].ch; }

}
