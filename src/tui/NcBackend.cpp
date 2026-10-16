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
		mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION | BUTTON_SHIFT | BUTTON_CTRL | BUTTON_ALT, nullptr);
		mouseinterval(0);
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

void NcBackend::readEvents(std::vector<Event>& out)
{
	struct Raw { wint_t code; bool special; };
	std::vector<Raw> raw;
	for (;;)
	{
		wint_t ch;
		const int r = get_wch(&ch);
		if (r == ERR)
			break;
		raw.push_back({ ch, r == KEY_CODE_YES });
	}

	for (size_t i = 0; i < raw.size(); i++)
	{
		const Raw& r = raw[i];

		// bracketed paste: ESC [ 2 0 0 ~ ... ESC [ 2 0 1 ~
		if (!r.special && r.code == 27 && i + 5 < raw.size() + 0 && raw[i + 1].code == '[' && raw[i + 2].code == '2' && raw[i + 3].code == '0' && raw[i + 4].code == '0' && raw[i + 5].code == '~')
		{
			QString text;
			size_t j = i + 6;
			for (; j < raw.size(); j++)
			{
				if (raw[j].code == 27 && j + 5 < raw.size() + 0 && raw[j + 1].code == '[' && raw[j + 2].code == '2' && raw[j + 3].code == '0' && raw[j + 4].code == '1' && raw[j + 5].code == '~')
					break;
				if (!raw[j].special)
					text += QString::fromUcs4(reinterpret_cast<const char32_t*>(&raw[j].code), 1);
			}
			out.push_back(Event::pasteEvent(text));
			i = j + 5;
			continue;
		}

		if (r.special && r.code == KEY_RESIZE)
		{
			Event e;
			e.type = Event::Resize;
			out.push_back(e);
			continue;
		}

		if (r.special && r.code == KEY_MOUSE)
		{
			MEVENT m;
			if (getmouse(&m) != OK)
				continue;
			const mmask_t b = m.bstate;
			Event e;
			e.type = Event::Mouse;
			e.x = m.x;
			e.y = m.y;
			e.ctrl = b & BUTTON_CTRL;
			e.shift = b & BUTTON_SHIFT;
			e.alt = b & BUTTON_ALT;
			const qint64 now = QDateTime::currentMSecsSinceEpoch();

			auto press = [&](int button) {
				e.mouse = Event::Press;
				e.button = button;
				e.doubleClick = button == 1 && m.x == m_lastPressX && m.y == m_lastPressY && now - m_lastPressMs < 400;
				if (button == 1)
				{
					m_lastPressMs = e.doubleClick ? 0 : now;
					m_lastPressX = m.x;
					m_lastPressY = m.y;
				}
				m_buttonDown = true;
				out.push_back(e);
			};
			if (b & BUTTON1_PRESSED) press(1);
			else if (b & BUTTON2_PRESSED) press(2);
			else if (b & BUTTON3_PRESSED) press(3);
			else if (b & BUTTON4_PRESSED) { e.mouse = Event::WheelUp; out.push_back(e); }
#ifdef BUTTON5_PRESSED
			else if (b & BUTTON5_PRESSED) { e.mouse = Event::WheelDown; out.push_back(e); }
#endif
			else if (b & (BUTTON1_RELEASED | BUTTON2_RELEASED | BUTTON3_RELEASED))
			{
				e.mouse = Event::Release;
				e.button = (b & BUTTON1_RELEASED) ? 1 : (b & BUTTON2_RELEASED) ? 2 : 3;
				m_buttonDown = false;
				out.push_back(e);
			}
			else if ((b & REPORT_MOUSE_POSITION) && m_buttonDown)
			{
				e.mouse = Event::Drag;
				out.push_back(e);
			}
			continue;
		}

		Event e = keyFromCode(int(r.code), r.special);
		if (e.type == Event::Key)
			out.push_back(e);
	}
}

}
