#include "Dialog.h"
#include "Format.h"
#include <algorithm>

namespace tui
{

static bool keyLeadsLabel(const QString& key, const QString& label)
{
	return key.size() == 1 && label.startsWith(key, Qt::CaseInsensitive);
}

int hintWidth(const QString& key, const QString& label)
{
	if (keyLeadsLabel(key, label))
		return displayWidth(label);
	return displayWidth(key) + 1 + displayWidth(label);
}

int drawHint(Canvas& c, int x, int y, const QString& key, const QString& label, Style base, bool enabled)
{
	const Style keyStyle = enabled ? Style::KeyHint : Style::Dim;
	const Style textStyle = enabled ? base : Style::Dim;
	if (keyLeadsLabel(key, label))
	{
		c.put(x, y, label.left(1), keyStyle);
		c.put(x + 1, y, label.mid(1), textStyle);
		return displayWidth(label);
	}
	c.put(x, y, key, keyStyle);
	c.put(x + displayWidth(key), y, " " + label, textStyle);
	return displayWidth(key) + 1 + displayWidth(label);
}

// ------------------------------------------------------------------ base

Rect Dialog::place(const Rect& screen, int w, int h)
{
	w = qMin(w, screen.w);
	h = qMin(h, screen.h);
	return { screen.x + (screen.w - w) / 2, screen.y + (screen.h - h) / 2, w, h };
}

void Dialog::beginDraw(HitMap& hits, const Rect& screen)
{
	m_clicks.clear();
	m_screen = screen;
	hits.block(screen);
}

void Dialog::drawFrame(Canvas& c, const QString& title)
{
	c.fill(m_rect, Style::Normal);
	c.box(m_rect, Style::BorderFocus, title, Style::Hl);
}

void Dialog::drawHints(Canvas& c, int x, int y, int w, const QVector<Hint>& hints)
{
	c.fill({ x, y, w, 1 }, Style::KeyBar);
	int cx = x + 1;
	for (const Hint& h : hints)
	{
		const int hw = hintWidth(h.key, h.label);
		if (cx + hw > x + w)
			break;
		drawHint(c, cx, y, h.key, h.label, Style::KeyBar);
		if (h.fn)
			addClick({ cx, y, hw, 1 }, h.fn);
		cx += hw + 2;
	}
}

bool Dialog::handleMouse(const Event& e)
{
	if (e.type != Event::Mouse)
		return false;
	if (e.mouse == Event::Press && (e.button == 1 || e.button == 3))
	{
		if (!m_rect.contains(e.x, e.y))
		{
			outsideClick();
			return true;
		}
		if (e.button == 1)
			for (int i = m_clicks.size() - 1; i >= 0; i--)
				if (m_clicks[i].r.contains(e.x, e.y))
				{
					auto fn = m_clicks[i].fn;
					if (fn)
						fn();
					return true;
				}
	}
	return true;
}

// ------------------------------------------------------------------ form

FormDialog::FormDialog(const QString& title, int width) : m_title(title), m_width(width)
{
}

FormField& FormDialog::addText(const QString& id, const QString& label, const QString& value, const QString& suffix)
{
	FormField f;
	f.type = FormField::Text;
	f.id = id; f.label = label; f.text = value; f.suffix = suffix;
	f.cursor = value.size();
	m_fields.push_back(f);
	return m_fields.last();
}

FormField& FormDialog::addSelect(const QString& id, const QString& label, const QStringList& options, const QStringList& values, int index)
{
	FormField f;
	f.type = FormField::Select;
	f.id = id; f.label = label; f.options = options; f.values = values;
	f.index = qBound(0, index, qMax(0, int(options.size()) - 1));
	m_fields.push_back(f);
	return m_fields.last();
}

FormField& FormDialog::addCheck(const QString& id, const QString& label, bool checked)
{
	FormField f;
	f.type = FormField::Check;
	f.id = id; f.label = label; f.checked = checked;
	m_fields.push_back(f);
	return m_fields.last();
}

FormField& FormDialog::addRadio(const QString& id, const QString& label, const QStringList& options, const QString& hotkeys, int index)
{
	FormField f;
	f.type = FormField::Radio;
	f.id = id; f.label = label; f.options = options; f.hotkeys = hotkeys; f.index = index;
	m_fields.push_back(f);
	return m_fields.last();
}

void FormDialog::addLabel(const QString& text)
{
	FormField f;
	f.type = FormField::Label;
	f.text = text;
	m_fields.push_back(f);
}

void FormDialog::addSpacer()
{
	FormField f;
	f.type = FormField::Spacer;
	m_fields.push_back(f);
}

FormField* FormDialog::field(const QString& id)
{
	for (FormField& f : m_fields)
		if (f.id == id)
			return &f;
	return nullptr;
}

QString FormDialog::text(const QString& id) const
{
	for (const FormField& f : m_fields)
		if (f.id == id)
			return f.text;
	return QString();
}

void FormDialog::setText(const QString& id, const QString& value)
{
	if (FormField* f = field(id))
	{
		f->text = value;
		f->cursor = value.size();
		f->scroll = 0;
	}
}

int FormDialog::index(const QString& id) const
{
	for (const FormField& f : m_fields)
		if (f.id == id)
			return f.index;
	return -1;
}

QString FormDialog::value(const QString& id) const
{
	for (const FormField& f : m_fields)
		if (f.id == id)
			return f.index >= 0 && f.index < f.values.size() ? f.values[f.index] : QString();
	return QString();
}

bool FormDialog::checked(const QString& id) const
{
	for (const FormField& f : m_fields)
		if (f.id == id)
			return f.checked;
	return false;
}

void FormDialog::setSubmit(const QString& label, std::function<bool(FormDialog&)> fn)
{
	m_submitLabel = label;
	m_onSubmit = std::move(fn);
}

void FormDialog::addShortcut(int key, bool ctrl, const QString& keyLabel, const QString& label, std::function<void(FormDialog&)> fn)
{
	m_shortcuts.push_back({ key, ctrl, keyLabel, label, std::move(fn) });
}

void FormDialog::focusField(const QString& id)
{
	for (int i = 0; i < m_fields.size(); i++)
		if (m_fields[i].id == id)
			m_focus = i;
}

QStringList FormDialog::wrap(const QString& text, int width) const
{
	QStringList out;
	for (const QString& para : text.split('\n'))
	{
		QString line;
		for (const QString& word : para.split(' '))
		{
			if (!line.isEmpty() && displayWidth(line) + 1 + displayWidth(word) > width)
			{
				out << line;
				line.clear();
			}
			line += (line.isEmpty() ? "" : " ") + word;
		}
		out << line;
	}
	return out;
}

int FormDialog::fieldHeight(const FormField& f, int innerWidth) const
{
	switch (f.type)
	{
		case FormField::Radio: return qMax(1, int(f.options.size()));
		case FormField::Label: return wrap(f.text, innerWidth).size();
		default: return 1;
	}
}

bool FormDialog::focusedIsText() const
{
	return m_focus >= 0 && m_focus < m_fields.size() && m_fields[m_focus].type == FormField::Text;
}

void FormDialog::draw(Canvas& c, HitMap& hits, const Rect& screen)
{
	beginDraw(hits, screen);

	// start in the first text field: that is what is usually typed into
	if (!m_focusInit)
	{
		m_focusInit = true;
		for (int i = 0; i < m_fields.size(); i++)
			if (m_fields[i].type == FormField::Text)
			{
				m_focus = i;
				break;
			}
	}
	// make sure the focus is on something focusable
	if (m_focus < 0 || m_focus >= m_fields.size() || !m_fields[m_focus].focusable())
	{
		m_focus = 0;
		while (m_focus < m_fields.size() && !m_fields[m_focus].focusable())
			m_focus++;
	}

	const int w = qMin(m_width, screen.w);
	const int innerW = w - 4;
	int labelW = 0;
	for (const FormField& f : m_fields)
		if (!f.label.isEmpty())
			labelW = qMax(labelW, displayWidth(f.label));
	labelW = qMin(labelW + 1, innerW / 2);
	const int valueW = innerW - labelW;

	int h = 5;
	for (const FormField& f : m_fields)
		h += fieldHeight(f, innerW);
	m_rect = place(screen, w, h);
	drawFrame(c, m_title);
	c.setCursor(0, 0, false);

	const int x0 = m_rect.x + 2, vx = x0 + labelW;
	int y = m_rect.y + 2;

	for (int i = 0; i < m_fields.size(); i++)
	{
		FormField& f = m_fields[i];
		const bool focus = i == m_focus;
		const int fh = fieldHeight(f, innerW);

		if (f.type == FormField::Label)
		{
			int ly = y;
			for (const QString& line : wrap(f.text, innerW))
				c.put(x0, ly++, fit(line, innerW), Style::Dim);
			y += fh;
			continue;
		}
		if (f.type == FormField::Spacer)
		{
			y += fh;
			continue;
		}

		c.put(x0, y, fit(f.label, labelW - 1), focus ? Style::Hl : Style::Dim);

		if (f.type == FormField::Text)
		{
			const int suffixW = f.suffix.isEmpty() ? 0 : displayWidth(f.suffix) + 1;
			const int fw = qMax(4, valueW - suffixW);

			// scroll so that the cursor stays visible
			f.cursor = qBound(0, f.cursor, int(f.text.size()));
			if (f.scroll > f.cursor)
				f.scroll = f.cursor;
			while (f.scroll < f.cursor && displayWidth(f.text.mid(f.scroll, f.cursor - f.scroll)) >= fw)
				f.scroll++;

			const int from = focus ? f.scroll : 0; // show the beginning of fields that are not being edited
			QString shown = f.secret ? repeat(Glyphs::get().dot, f.text.size() - from) : f.text.mid(from);
			if (f.text.isEmpty() && !focus && !f.placeholder.isEmpty())
			{
				c.put(vx, y, fit(f.placeholder, fw), Style::Dim);
			}
			else
			{
				c.put(vx, y, fit(shown, fw), Style::Input);
			}
			if (suffixW)
				c.put(vx + fw + 1, y, f.suffix, Style::Dim);
			if (focus)
			{
				const QString before = f.secret ? repeat(Glyphs::get().dot, f.cursor - f.scroll) : f.text.mid(f.scroll, f.cursor - f.scroll);
				c.setCursor(vx + displayWidth(before), y, true);
			}
			addClick({ vx, y, fw, 1 }, [this, i]() { m_focus = i; }); // the cursor is placed in handle()
		}
		else if (f.type == FormField::Select)
		{
			int optW = 0;
			for (const QString& o : f.options)
				optW = qMax(optW, displayWidth(o));
			optW = qMin(optW, valueW - 4);
			const Glyphs& g = Glyphs::get();
			const QString cur = f.options.value(f.index);
			const Style st = focus ? Style::Selected : Style::Input;
			c.put(vx, y, g.left + " ", st);
			c.put(vx + 2, y, fit(cur, optW), st);
			c.put(vx + 2 + optW, y, " " + g.right, st);
			addClick({ vx, y, 2, 1 }, [this, i, &f]() { m_focus = i; changeSelect(f, -1); });
			addClick({ vx + 2, y, optW, 1 }, [this, i]() { m_focus = i; });
			addClick({ vx + 2 + optW, y, 2, 1 }, [this, i, &f]() { m_focus = i; changeSelect(f, 1); });
		}
		else if (f.type == FormField::Check)
		{
			const Glyphs& g = Glyphs::get();
			c.put(vx, y, f.checked ? g.checkOn : g.checkOff, focus ? Style::Selected : Style::Input);
			addClick({ vx, y, 3, 1 }, [this, i, &f]() { m_focus = i; f.checked = !f.checked; if (f.onChange) f.onChange(); });
		}
		else if (f.type == FormField::Radio)
		{
			const Glyphs& g = Glyphs::get();
			for (int o = 0; o < f.options.size(); o++)
			{
				const bool on = o == f.index;
				c.put(vx, y + o, on ? g.radioOn : g.radioOff, on && focus ? Style::Hl : Style::Normal);
				int tx = vx + displayWidth(on ? g.radioOn : g.radioOff) + 1;
				if (o < f.hotkeys.size())
				{
					c.put(tx, y + o, f.hotkeys.mid(o, 1), Style::KeyHint);
					tx += 2;
				}
				c.put(tx, y + o, fit(f.options[o], vx + valueW - tx), on ? Style::Bold : Style::Normal);
				addClick({ vx, y + o, valueW, 1 }, [this, i, o, &f]() { m_focus = i; f.index = o; if (f.onChange) f.onChange(); });
			}
		}
		y += fh;
	}

	// error line and the hints
	if (!m_error.isEmpty())
		c.put(x0, m_rect.bottom() - 3, fit(m_error, innerW), Style::Red);

	QVector<Hint> hints;
	hints.push_back({ QString::fromUtf8("⏎"), m_submitLabel, [this]() { submit(); } });
	int nfocus = 0;
	for (const FormField& f : m_fields)
		nfocus += f.focusable();
	if (nfocus > 1)
		hints.push_back({ "Tab", "Next", [this]() { moveFocus(1); } });
	for (const Shortcut& s : m_shortcuts)
		hints.push_back({ s.keyLabel, s.label, [this, &s]() { s.fn(*this); } });
	hints.push_back({ "Esc", "Cancel", [this]() { cancel(); } });
	drawHints(c, m_rect.x + 1, m_rect.bottom() - 2, m_rect.w - 2, hints);
}

void FormDialog::moveFocus(int dir)
{
	if (m_fields.isEmpty())
		return;
	for (int n = 0; n < m_fields.size(); n++)
	{
		m_focus = (m_focus + dir + m_fields.size()) % m_fields.size();
		if (m_fields[m_focus].focusable())
			return;
	}
}

void FormDialog::changeSelect(FormField& f, int dir)
{
	if (f.options.isEmpty())
		return;
	f.index = (f.index + dir + f.options.size()) % f.options.size();
	if (f.onChange)
		f.onChange();
}

void FormDialog::submit()
{
	if (!m_onSubmit || m_onSubmit(*this))
		close();
}

void FormDialog::cancel()
{
	if (m_onCancel)
		m_onCancel();
	close();
}

void FormDialog::handleText(FormField& f, const Event& e)
{
	f.cursor = qBound(0, f.cursor, int(f.text.size()));
	if (e.ctrl)
	{
		switch (QChar(e.key).toLower().unicode())
		{
			case 'u': f.text.clear(); f.cursor = 0; break;
			case 'a': f.cursor = 0; break;
			case 'e': f.cursor = f.text.size(); break;
			case 'k': f.text.truncate(f.cursor); break;
			case 'w':
			{
				int p = f.cursor;
				while (p > 0 && f.text[p - 1] == ' ') p--;
				while (p > 0 && f.text[p - 1] != ' ') p--;
				f.text.remove(p, f.cursor - p);
				f.cursor = p;
				break;
			}
		}
		return;
	}
	switch (e.key)
	{
		case KeyLeft: f.cursor = qMax(0, f.cursor - 1); break;
		case KeyRight: f.cursor = qMin(int(f.text.size()), f.cursor + 1); break;
		case KeyHome: f.cursor = 0; break;
		case KeyEnd: f.cursor = f.text.size(); break;
		case KeyBackspace:
			if (f.cursor > 0)
				f.text.remove(--f.cursor, 1);
			break;
		case KeyDelete:
			if (f.cursor < f.text.size())
				f.text.remove(f.cursor, 1);
			break;
		default:
			if (e.isPrintable())
			{
				const QString s = QString::fromUcs4(reinterpret_cast<const char32_t*>(&e.key), 1);
				f.text.insert(f.cursor, s);
				f.cursor += s.size();
			}
	}
}

void FormDialog::handle(const Event& e)
{
	if (e.type == Event::Mouse)
	{
		handleMouse(e);
		// a click in a text field also places the cursor at the pressed column
		if (e.mouse == Event::Press && e.button == 1 && focusedIsText())
		{
			FormField& f = m_fields[m_focus];
			// the field's left edge equals the left edge of its click rectangle
			for (const Click& cl : m_clicks)
				if (cl.r.contains(e.x, e.y) && cl.r.h == 1)
				{
					int col = e.x - cl.r.x, w = 0, pos = f.scroll;
					while (pos < f.text.size())
					{
						int cw = displayWidth(f.text.mid(pos, 1));
						if (w + cw > col)
							break;
						w += cw;
						pos++;
					}
					f.cursor = pos;
					break;
				}
		}
		return;
	}

	if (e.type == Event::Paste)
	{
		if (focusedIsText())
		{
			FormField& f = m_fields[m_focus];
			QString t = e.text;
			t.replace('\r', ' ').replace('\n', ' ');
			f.text.insert(f.cursor, t);
			f.cursor += t.size();
		}
		return;
	}

	if (e.type != Event::Key)
		return;

	if (e.is(KeyEscape))
		return cancel();
	if (e.is(KeyEnter))
		return submit();
	if (e.is(KeyTab))
		return moveFocus(1);
	if (e.is(KeyBackTab))
		return moveFocus(-1);

	for (const Shortcut& s : m_shortcuts)
		if (e.type == Event::Key && s.ctrl == e.ctrl && QChar(e.key).toLower().unicode() == QChar(s.key).toLower().unicode() && !e.alt)
			return s.fn(*this);

	if (m_focus < 0 || m_focus >= m_fields.size())
		return;
	FormField& f = m_fields[m_focus];

	if (f.type == FormField::Text)
	{
		if (e.key == KeyUp)
			return moveFocus(-1);
		if (e.key == KeyDown)
			return moveFocus(1);
		return handleText(f, e);
	}

	// letter shortcuts of radio groups
	if (e.isPrintable())
		for (int i = 0; i < m_fields.size(); i++)
		{
			FormField& r = m_fields[i];
			if (r.type != FormField::Radio)
				continue;
			const int o = r.hotkeys.indexOf(QChar(e.key).toLower(), 0, Qt::CaseInsensitive);
			if (o >= 0)
			{
				r.index = o;
				m_focus = i;
				if (r.onChange)
					r.onChange();
				return;
			}
		}

	if (f.type == FormField::Radio)
	{
		if (e.key == KeyUp)
		{
			if (f.index > 0) f.index--; else moveFocus(-1);
			if (f.onChange) f.onChange();
		}
		else if (e.key == KeyDown)
		{
			if (f.index < f.options.size() - 1) f.index++; else moveFocus(1);
			if (f.onChange) f.onChange();
		}
		return;
	}

	if (e.key == KeyUp)
		return moveFocus(-1);
	if (e.key == KeyDown)
		return moveFocus(1);
	if (f.type == FormField::Select)
	{
		if (e.key == KeyLeft) changeSelect(f, -1);
		else if (e.key == KeyRight || e.key == ' ') changeSelect(f, 1);
	}
	else if (f.type == FormField::Check && e.key == ' ')
	{
		f.checked = !f.checked;
		if (f.onChange)
			f.onChange();
	}
}

// ---------------------------------------------------------------- choices

ChoiceDialog::ChoiceDialog(const QString& title, const QStringList& message, const QVector<Choice>& choices, std::function<void(int)> done)
	: m_title(title), m_message(message), m_choices(choices), m_done(std::move(done))
{
}

void ChoiceDialog::finish(int result)
{
	if (m_finished)
		return;
	close();
	if (m_done)
		m_done(result);
}

void ChoiceDialog::draw(Canvas& c, HitMap& hits, const Rect& screen)
{
	beginDraw(hits, screen);
	int w = displayWidth(m_title) + 8;
	for (const QString& l : m_message)
		w = qMax(w, displayWidth(l) + 6);
	for (const Choice& ch : m_choices)
		w = qMax(w, hintWidth(ch.key, ch.label) + 8);
	w = qMax(w, 36);
	const int h = m_message.size() + m_choices.size() + 5;
	m_rect = place(screen, w, h);
	drawFrame(c, m_title);

	int y = m_rect.y + 1;
	for (const QString& l : m_message)
		c.put(m_rect.x + 2, ++y - 0, fit(l, m_rect.w - 4), Style::Normal);
	y += 1;
	for (const Choice& ch : m_choices)
	{
		++y;
		const int hw = drawHint(c, m_rect.x + 3, y, ch.key, ch.label, Style::Normal);
		const int result = ch.result;
		addClick({ m_rect.x + 2, y, hw + 2, 1 }, [this, result]() { finish(result); });
	}
	const int hy = m_rect.bottom() - 2;
	c.fill({ m_rect.x + 1, hy, m_rect.w - 2, 1 }, Style::KeyBar);
	drawHint(c, m_rect.x + 2, hy, "Esc", "Cancel", Style::KeyBar);
	addClick({ m_rect.x + 2, hy, hintWidth("Esc", "Cancel"), 1 }, [this]() { finish(-1); });
}

void ChoiceDialog::handle(const Event& e)
{
	if (handleMouse(e) && e.type == Event::Mouse)
		return;
	if (e.type != Event::Key)
		return;
	if (e.is(KeyEscape))
		return finish(-1);
	for (const Choice& ch : m_choices)
		if (e.isLetter(ch.key.at(0).toLower().toLatin1()))
			return finish(ch.result);
}

// ------------------------------------------------------------------- menu

MenuDialog::MenuDialog(const QVector<MenuItem>& items, int x, int y, const QString& title)
	: m_items(items), m_title(title), m_x(x), m_y(y)
{
	move(0);
}

void MenuDialog::move(int dir)
{
	const int n = m_items.size();
	if (n == 0)
		return;
	if (dir == 0)
	{
		m_sel = qBound(0, m_sel, n - 1);
		dir = 1;
		if (!m_items[m_sel].separator && m_items[m_sel].enabled)
			return;
	}
	for (int i = 0; i < n; i++)
	{
		m_sel = (m_sel + dir + n) % n;
		if (!m_items[m_sel].separator && m_items[m_sel].enabled)
			return;
	}
}

void MenuDialog::choose(int i)
{
	if (i < 0 || i >= m_items.size() || m_items[i].separator || !m_items[i].enabled)
		return;
	auto fn = m_items[i].fn;
	close();
	if (fn)
		fn();
}

void MenuDialog::draw(Canvas& c, HitMap& hits, const Rect& screen)
{
	beginDraw(hits, screen);
	int labelW = displayWidth(m_title) + 2, keyW = 0;
	for (const MenuItem& i : m_items)
	{
		labelW = qMax(labelW, displayWidth(i.label));
		keyW = qMax(keyW, displayWidth(i.key));
	}
	const int w = labelW + keyW + 5;
	const int h = m_items.size() + 2;
	m_rect = { qBound(screen.x, m_x, qMax(screen.x, screen.right() - w)), qBound(screen.y, m_y, qMax(screen.y, screen.bottom() - h)), qMin(w, screen.w), qMin(h, screen.h) };
	drawFrame(c, m_title);
	const Glyphs& g = Glyphs::get();
	for (int i = 0; i < m_items.size(); i++)
	{
		const MenuItem& it = m_items[i];
		const int y = m_rect.y + 1 + i;
		if (y >= m_rect.bottom() - 1)
			break;
		if (it.separator)
		{
			c.put(m_rect.x, y, g.teeL + repeat(g.h, m_rect.w - 2) + g.teeR, Style::BorderFocus);
			continue;
		}
		const Style st = !it.enabled ? Style::Dim : (i == m_sel ? Style::MenuSel : Style::Normal);
		c.fill({ m_rect.x + 1, y, m_rect.w - 2, 1 }, st);
		c.put(m_rect.x + 2, y, it.label, st);
		c.put(m_rect.right() - 2 - displayWidth(it.key), y, it.key, st);
		addClick({ m_rect.x + 1, y, m_rect.w - 2, 1 }, [this, i]() { choose(i); });
	}
}

void MenuDialog::handle(const Event& e)
{
	if (e.type == Event::Mouse)
	{
		// hovering is not tracked; a press chooses
		handleMouse(e);
		return;
	}
	if (e.type != Event::Key)
		return;
	if (e.is(KeyEscape))
		return close();
	if (e.key == KeyUp || e.isLetter('k'))
		return move(-1);
	if (e.key == KeyDown || e.isLetter('j'))
		return move(1);
	if (e.is(KeyEnter))
		return choose(m_sel);
	if (e.isPrintable())
		for (int i = 0; i < m_items.size(); i++)
		{
			const QString& k = m_items[i].key;
			if (k.size() == 1 && QChar(e.key).toLower() == k[0].toLower())
				return choose(i);
		}
}

// ------------------------------------------------------------------- text

TextDialog::TextDialog(const QString& title, const QStringList& lines, int width)
	: m_title(title), m_lines(lines), m_width(width)
{
}

void TextDialog::draw(Canvas& c, HitMap& hits, const Rect& screen)
{
	beginDraw(hits, screen);
	const int h = qMin(int(m_lines.size()) + 4, screen.h - 2);
	m_rect = place(screen, m_width, h);
	drawFrame(c, m_title);
	m_visible = m_rect.h - 4;
	m_top = qBound(0, m_top, qMax(0, int(m_lines.size()) - m_visible));
	for (int i = 0; i < m_visible && m_top + i < m_lines.size(); i++)
		c.put(m_rect.x + 2, m_rect.y + 2 + i, fit(m_lines[m_top + i], m_rect.w - 4), Style::Normal);
	if (m_lines.size() > m_visible)
	{
		const Glyphs& g = Glyphs::get();
		const int th = qMax(1, m_visible * m_visible / int(m_lines.size()));
		const int tp = m_top * (m_visible - th) / qMax(1, int(m_lines.size()) - m_visible);
		for (int i = 0; i < m_visible; i++)
			c.put(m_rect.right() - 2, m_rect.y + 2 + i, i >= tp && i < tp + th ? g.scrollThumb : g.scrollTrack, Style::Dim);
	}
	drawHints(c, m_rect.x + 1, m_rect.bottom() - 2, m_rect.w - 2, {
		{ QString::fromUtf8("↑↓"), "Scroll", nullptr }, { "Esc", "Close", [this]() { close(); } } });
}

void TextDialog::handle(const Event& e)
{
	if (e.type == Event::Mouse)
	{
		if (e.mouse == Event::WheelUp) m_top = qMax(0, m_top - 3);
		else if (e.mouse == Event::WheelDown) m_top += 3;
		else handleMouse(e);
		return;
	}
	if (e.type != Event::Key)
		return;
	if (e.is(KeyEscape) || e.is(KeyEnter) || e.isLetter('q'))
		return close();
	if (e.key == KeyUp || e.isLetter('k')) m_top = qMax(0, m_top - 1);
	else if (e.key == KeyDown || e.isLetter('j')) m_top++;
	else if (e.key == KeyPageUp) m_top = qMax(0, m_top - m_visible);
	else if (e.key == KeyPageDown) m_top += m_visible;
	else if (e.key == KeyHome) m_top = 0;
	else if (e.key == KeyEnd) m_top = 1 << 20;
}

}
