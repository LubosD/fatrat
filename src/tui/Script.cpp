#include "Script.h"
#include <QDateTime>
#include <QTimer>
#include <cstdio>

namespace tui
{

ScriptRunner::ScriptRunner(App& app, TextCanvas& canvas, const QStringList& lines, QObject* parent)
	: QObject(parent), m_app(app), m_canvas(canvas), m_lines(lines)
{
}

void ScriptRunner::start()
{
	QTimer::singleShot(0, this, &ScriptRunner::next);
}

bool ScriptRunner::parseKey(const QString& name, Event& out)
{
	static const QMap<QString, int> names = {
		{ "enter", KeyEnter }, { "esc", KeyEscape }, { "escape", KeyEscape }, { "tab", KeyTab }, { "backtab", KeyBackTab },
		{ "up", KeyUp }, { "down", KeyDown }, { "left", KeyLeft }, { "right", KeyRight }, { "pgup", KeyPageUp }, { "pgdn", KeyPageDown },
		{ "home", KeyHome }, { "end", KeyEnd }, { "backspace", KeyBackspace }, { "delete", KeyDelete }, { "space", ' ' },
	};
	const QString n = name.toLower();
	if (names.contains(n))
	{
		out = Event::keyEvent(names[n]);
		return true;
	}
	if (n.startsWith("ctrl-") && n.size() == 6)
	{
		out = Event::keyEvent(n[5].unicode(), true);
		return true;
	}
	if (n.size() >= 2 && n[0] == 'f' && n.mid(1).toInt() >= 1 && n.mid(1).toInt() <= 12)
	{
		out = Event::keyEvent(KeyF1 + n.mid(1).toInt() - 1);
		return true;
	}
	if (name.size() == 1)
	{
		out = Event::charEvent(name[0]);
		return true;
	}
	return false;
}

QString ScriptRunner::screenText()
{
	m_canvas.clear();
	m_app.draw(m_canvas);
	return m_canvas.toText();
}

void ScriptRunner::redraw()
{
	screenText();
}

void ScriptRunner::fail(const QString& msg)
{
	fprintf(stderr, "SCRIPT FAILED at line %d: %s\n%s\n", m_pos, qPrintable(msg), qPrintable(screenText()));
	m_rc = 1;
}

void ScriptRunner::next()
{
	// waiting for a text to appear
	if (!m_waitText.isEmpty())
	{
		if (screenText().contains(m_waitText))
			m_waitText.clear();
		else if (QDateTime::currentMSecsSinceEpoch() > m_waitDeadline)
		{
			fail(QString("timed out waiting for \"%1\"").arg(m_waitText));
			emit finished(m_rc);
			return;
		}
		else
		{
			QTimer::singleShot(50, this, &ScriptRunner::next);
			return;
		}
	}

	while (m_pos < m_lines.size())
	{
		const QString line = m_lines[m_pos++].trimmed();
		if (line.isEmpty() || line.startsWith('#'))
			continue;
		const QString cmd = line.section(' ', 0, 0);
		const QString arg = line.section(' ', 1);

		if (cmd == "wait")
		{
			QTimer::singleShot(arg.toInt(), this, &ScriptRunner::next);
			return;
		}
		if (cmd == "waitfor")
		{
			QString text = arg;
			int timeout = 8000;
			const int sep = arg.lastIndexOf(" @");
			if (sep >= 0)
			{
				text = arg.left(sep);
				timeout = arg.mid(sep + 2).toInt();
			}
			m_waitText = text;
			m_waitDeadline = QDateTime::currentMSecsSinceEpoch() + timeout;
			next();
			return;
		}
		if (!run(line))
		{
			emit finished(m_rc);
			return;
		}
	}
	emit finished(m_rc);
}

bool ScriptRunner::run(const QString& line)
{
	const QString cmd = line.section(' ', 0, 0);
	const QString arg = line.section(' ', 1);

	if (cmd == "key")
	{
		Event e;
		if (!parseKey(arg, e))
		{
			fail("unknown key " + arg);
			return false;
		}
		screenText();
		m_app.handle(e);
		screenText();
	}
	else if (cmd == "type")
	{
		for (const QChar c : arg)
		{
			screenText();
			m_app.handle(Event::charEvent(c));
		}
		screenText();
	}
	else if (cmd == "paste")
	{
		screenText();
		m_app.handle(Event::pasteEvent(arg));
		screenText();
	}
	else if (cmd == "mouse")
	{
		const QStringList a = arg.split(' ', Qt::SkipEmptyParts);
		if (a.size() < 3)
		{
			fail("mouse needs a kind and a position");
			return false;
		}
		static const QMap<QString, Event::MouseKind> kinds = {
			{ "press", Event::Press }, { "release", Event::Release }, { "drag", Event::Drag },
			{ "wheelup", Event::WheelUp }, { "wheeldown", Event::WheelDown },
		};
		if (!kinds.contains(a[0]))
		{
			fail("unknown mouse event " + a[0]);
			return false;
		}
		Event e = Event::mouseEvent(kinds[a[0]], a[1].toInt(), a[2].toInt(), a.size() > 3 && a[3][0].isDigit() ? a[3].toInt() : 1);
		for (const QString& m : a.mid(3))
		{
			e.ctrl |= m == "ctrl";
			e.shift |= m == "shift";
			e.doubleClick |= m == "double";
		}
		screenText();
		m_app.handle(e);
		screenText();
	}
	else if (cmd == "expect")
	{
		if (!screenText().contains(arg))
		{
			fail(QString("expected \"%1\" on the screen").arg(arg));
			return false;
		}
	}
	else if (cmd == "reject")
	{
		if (screenText().contains(arg))
		{
			fail(QString("did not expect \"%1\" on the screen").arg(arg));
			return false;
		}
	}
	else if (cmd == "dump")
	{
		const QString text = screenText();
		printf("---- %s\n", qPrintable(arg));
		for (QString l : text.split('\n'))
		{
			while (l.endsWith(' '))
				l.chop(1);
			printf("%s\n", qPrintable(l));
		}
		fflush(stdout);
	}
	else
	{
		fail("unknown command " + cmd);
		return false;
	}
	return true;
}

}
