#include "NcBackend.h"
#include <QDateTime>
#include <clocale>
#include <cstdio>
#include <ncurses.h>

namespace tui
{

enum { PairBase = 1 };

static int attrFor(Style s)
{
	// colour pairs are numbered by style; the table is set up in init()
	return COLOR_PAIR(PairBase + int(s));
}

static void setupStyles()
{
	auto pair = [](Style s, short fg, short bg) { init_pair(PairBase + int(s), fg, bg); };
	use_default_colors();
	pair(Style::Normal, -1, -1);
	pair(Style::Dim, -1, -1);
	pair(Style::Bold, -1, -1);
	pair(Style::Hl, COLOR_CYAN, -1);
	pair(Style::Title, COLOR_BLACK, COLOR_WHITE);
	pair(Style::TabOn, COLOR_BLACK, COLOR_CYAN);
	pair(Style::Selected, COLOR_BLACK, COLOR_CYAN);
	pair(Style::SelectedDim, COLOR_BLACK, COLOR_WHITE);
	pair(Style::Header, -1, -1);
	pair(Style::Status, COLOR_WHITE, COLOR_BLUE);
	pair(Style::StatusWarn, COLOR_WHITE, COLOR_RED);
	pair(Style::KeyBar, COLOR_BLACK, COLOR_WHITE);
	pair(Style::KeyHint, COLOR_RED, COLOR_WHITE);
	pair(Style::Green, COLOR_GREEN, -1);
	pair(Style::Yellow, COLOR_YELLOW, -1);
	pair(Style::Red, COLOR_RED, -1);
	pair(Style::Cyan, COLOR_CYAN, -1);
	pair(Style::Input, COLOR_WHITE, COLOR_BLUE);
	pair(Style::Mark, COLOR_YELLOW, -1);
	pair(Style::Border, -1, -1);
	pair(Style::BorderFocus, COLOR_CYAN, -1);
	pair(Style::Error, COLOR_WHITE, COLOR_RED);
	pair(Style::MenuSel, COLOR_BLACK, COLOR_CYAN);
}

static int extraAttrs(Style s)
{
	switch (s)
	{
		case Style::Dim: return A_DIM;
		case Style::Bold:
		case Style::Hl:
		case Style::TabOn:
		case Style::KeyHint:
		case Style::Mark:
		case Style::Error:
		case Style::StatusWarn: return A_BOLD;
		case Style::Header: return A_UNDERLINE | A_DIM;
		case Style::Border: return A_DIM;
		case Style::SelectedDim: return A_DIM;
		default: return 0;
	}
}

NcBackend::~NcBackend()
{
	shutdown();
}

bool NcBackend::init(bool mouse)
{
	setlocale(LC_ALL, "");
	if (!initscr())
		return false;
	cbreak();
	raw();                 // Ctrl-C arrives as a key, so the program can clean up
	noecho();
	nonl();
	keypad(stdscr, TRUE);
	nodelay(stdscr, TRUE);
	set_escdelay(25);
	curs_set(0);
	if (has_colors())
	{
		start_color();
		setupStyles();
	}
	m_mouse = mouse;
	if (mouse)
	{
		// We decode the xterm SGR mouse protocol ourselves: ncurses merges and reorders button events
		keyok(KEY_MOUSE, FALSE);
		fputs("\033[?1002h\033[?1006h", stdout);
	}
	// bracketed paste, so that pasted text does not act as key presses
	fputs("\033[?2004h", stdout);
	fflush(stdout);
	m_active = true;
	return true;
}

void NcBackend::shutdown()
{
	if (!m_active)
		return;
	m_active = false;
	fputs("\033[?2004l", stdout);
	if (m_mouse)
		fputs("\033[?1006l\033[?1002l", stdout);
	fflush(stdout);
	curs_set(1);
	endwin();
}

int NcBackend::width() const { return COLS; }
int NcBackend::height() const { return LINES; }

void NcBackend::put(int x, int y, const QString& text, Style style)
{
	if (y < 0 || y >= LINES || x >= COLS)
		return;
	QString t = Glyphs::isAscii() ? Glyphs::fold(text) : text;
	if (x < 0)
	{
		// skip the cells that are off the left edge
		int skip = -x;
		QString rest;
		for (const QChar ch : t)
		{
			if (skip > 0)
				skip--;
			else
				rest += ch;
		}
		t = rest;
		x = 0;
	}
	const bool colors = has_colors();
	const int attr = (colors ? attrFor(style) : 0) | extraAttrs(style)
		| (!colors && (style == Style::Selected || style == Style::Title || style == Style::KeyBar || style == Style::Status || style == Style::TabOn || style == Style::MenuSel) ? A_REVERSE : 0);
	attrset(attr);
	mvaddnstr(y, x, t.toUtf8().constData(), -1);
	attrset(A_NORMAL);
}

void NcBackend::setCursor(int x, int y, bool visible)
{
	m_cx = x;
	m_cy = y;
	m_cursorVisible = visible;
}

void NcBackend::begin()
{
	erase();
}

void NcBackend::flush()
{
	if (m_cursorVisible)
	{
		curs_set(1);
		move(m_cy, m_cx);
	}
	else
	{
		curs_set(0);
	}
	refresh();
}

static Event keyFromCode(int code, bool isKeyCode)
{
	Event e;
	e.type = Event::Key;
	if (isKeyCode)
	{
		switch (code)
		{
			case KEY_UP: e.key = KeyUp; break;
			case KEY_DOWN: e.key = KeyDown; break;
			case KEY_LEFT: e.key = KeyLeft; break;
			case KEY_RIGHT: e.key = KeyRight; break;
			case KEY_PPAGE: e.key = KeyPageUp; break;
			case KEY_NPAGE: e.key = KeyPageDown; break;
			case KEY_HOME: e.key = KeyHome; break;
			case KEY_END: e.key = KeyEnd; break;
			case KEY_ENTER: e.key = KeyEnter; break;
			case KEY_BTAB: e.key = KeyBackTab; break;
			case KEY_BACKSPACE: e.key = KeyBackspace; break;
			case KEY_DC: e.key = KeyDelete; break;
			case KEY_IC: e.key = KeyInsert; break;
			default:
				if (code >= KEY_F(1) && code <= KEY_F(12))
					e.key = KeyF1 + (code - KEY_F(1));
				else
					e.type = Event::None;
		}
		return e;
	}

	if (code == '\n' || code == '\r')
		e.key = KeyEnter;
	else if (code == '\t')
		e.key = KeyTab;
	else if (code == 27)
		e.key = KeyEscape;
	else if (code == 127 || code == 8)
		e.key = KeyBackspace;
	else if (code >= 1 && code <= 26)
	{
		e.key = 'a' + code - 1;
		e.ctrl = true;
	}
	else
		e.key = code;
	return e;
}

// Parses "ESC [ < b ; x ; y M|m"; returns the number of codes used, 0 if there is no such sequence,
// or -1 if the sequence is incomplete
static int parseSgrMouse(const std::vector<wint_t>& v, const std::vector<bool>& special, size_t i, int& b, int& x, int& y, bool& release)
{
	auto at = [&](size_t k) -> int { return k < v.size() && !special[k] ? int(v[k]) : -2; };
	if (at(i) != 27)
		return 0;
	if (i + 1 >= v.size())
		return -1;
	if (at(i + 1) != '[')
		return 0;
	if (i + 2 >= v.size())
		return -1;
	if (at(i + 2) != '<')
		return 0;
	size_t k = i + 3;
	int nums[3] = { 0, 0, 0 }, n = 0;
	bool any = false;
	for (; k < v.size(); k++)
	{
		const int c = at(k);
		if (c >= '0' && c <= '9')
		{
			nums[n] = nums[n] * 10 + (c - '0');
			any = true;
		}
		else if (c == ';' && any && n < 2)
		{
			n++;
			any = false;
		}
		else if ((c == 'M' || c == 'm') && any && n == 2)
		{
			b = nums[0];
			x = nums[1] - 1;
			y = nums[2] - 1;
			release = c == 'm';
			return int(k - i + 1);
		}
		else
			return 0;
	}
	return -1;
}

void NcBackend::readEvents(std::vector<Event>& out)
{
	// everything that is pending, including what an earlier call left over
	std::vector<wint_t> codes = m_leftCodes;
	const bool carried = !codes.empty();
	std::vector<bool> special;
	for (size_t i = 0; i < codes.size(); i++)
		special.push_back(false);
	m_leftCodes.clear();
	for (;;)
	{
		wint_t ch;
		const int r = get_wch(&ch);
		if (r == ERR)
			break;
		codes.push_back(ch);
		special.push_back(r == KEY_CODE_YES);
	}

	for (size_t i = 0; i < codes.size(); i++)
	{
		const bool sp = special[i];
		const wint_t code = codes[i];

		if (!sp && code == 27 && m_mouse)
		{
			int b, x, y;
			bool release;
			const int used = parseSgrMouse(codes, special, i, b, x, y, release);
			if (used > 0)
			{
				const qint64 now = QDateTime::currentMSecsSinceEpoch();
				Event e;
				e.type = Event::Mouse;
				e.x = x;
				e.y = y;
				e.shift = b & 4;
				e.alt = b & 8;
				e.ctrl = b & 16;
				const int low = b & 3;
				if (b & 64)
				{
					e.mouse = low == 1 ? Event::WheelDown : Event::WheelUp;
					if (!release)
						out.push_back(e);
				}
				else if (b & 32)
				{
					e.mouse = Event::Drag;
					e.button = low + 1;
					if (m_buttonDown)
						out.push_back(e);
				}
				else if (release)
				{
					e.mouse = Event::Release;
					e.button = low + 1;
					m_buttonDown = false;
					out.push_back(e);
				}
				else
				{
					e.mouse = Event::Press;
					e.button = low + 1;
					e.doubleClick = e.button == 1 && x == m_lastPressX && y == m_lastPressY && now - m_lastPressMs < 400;
					if (e.button == 1)
					{
						m_lastPressMs = e.doubleClick ? 0 : now;
						m_lastPressX = x;
						m_lastPressY = y;
					}
					m_buttonDown = true;
					out.push_back(e);
				}
				i += used - 1;
				continue;
			}
			if (used < 0 && i + 1 < codes.size() && !carried)
			{
				// an incomplete sequence at the end of this burst: finish it with the next one
				for (size_t k = i; k < codes.size(); k++)
					m_leftCodes.push_back(codes[k]);
				return;
			}
		}

		// bracketed paste: ESC [ 2 0 0 ~ ... ESC [ 2 0 1 ~
		auto seq = [&](size_t at, const char* text) {
			for (size_t k = 0; text[k]; k++)
				if (at + k >= codes.size() || special[at + k] || codes[at + k] != wint_t(text[k]))
					return false;
			return true;
		};
		if (!sp && code == 27 && seq(i + 1, "[200~"))
		{
			QString text;
			size_t j = i + 6;
			for (; j < codes.size(); j++)
			{
				if (codes[j] == 27 && seq(j + 1, "[201~"))
					break;
				if (!special[j])
					text += QString::fromUcs4(reinterpret_cast<const char32_t*>(&codes[j]), 1);
			}
			out.push_back(Event::pasteEvent(text));
			i = j + 5;
			continue;
		}

		if (sp && code == KEY_RESIZE)
		{
			Event e;
			e.type = Event::Resize;
			out.push_back(e);
			continue;
		}

		Event e = keyFromCode(int(code), sp);
		if (e.type == Event::Key)
			out.push_back(e);
	}
}

}
