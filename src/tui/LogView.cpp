#include "LogView.h"
#include "Dialogs.h"
#include "Format.h"

namespace tui
{

QVector<BarItem> LogView::bar() const
{
	QVector<BarItem> items = View::bar();
	for (BarItem& i : items)
		if (i.action == Action::Follow)
			i.label = m_follow ? m_app.tr("Follow (on)") : m_app.tr("Follow");
	return items;
}

QString LogView::statusText() const
{
	QString s = m_transferUuid.isEmpty() ? m_app.tr(" Global log") : m_app.tr(" Log of %1").arg(m_transferName);
	s += m_follow ? m_app.tr(" · following") : QString();
	if (!m_needle.isEmpty())
		s += m_app.tr(" · search: %1").arg(m_needle);
	return s;
}

void LogView::poll()
{
	const int gen = ++m_gen;
	const QString uuid = m_transferUuid;
	auto cb = [this, gen, uuid](const ApiResult& r) {
		if (gen != m_gen || uuid != m_transferUuid)
			return;
		if (!r.ok())
		{
			if (r.status == 404)
			{
				m_transferUuid.clear();
				m_app.toast(m_app.tr("That transfer is gone"), true);
			}
			return;
		}
		m_lines = QString::fromUtf8(r.raw).split('\n');
		while (!m_lines.isEmpty() && m_lines.last().isEmpty())
			m_lines.removeLast();
	};
	if (uuid.isEmpty())
		m_app.api().globalLog(cb);
	else
		m_app.api().transferLog(uuid, cb);
}

void LogView::draw(Canvas& c, HitMap& hits, const Rect& area)
{
	m_rows = area.h;
	const int total = m_lines.size();
	if (m_follow)
		m_top = qMax(0, total - m_rows);
	m_top = qBound(0, m_top, qMax(0, total - m_rows));

	const Glyphs& g = Glyphs::get();
	const bool bar = total > m_rows;
	const int w = area.w - (bar ? 1 : 0);
	for (int i = 0; i < m_rows && m_top + i < total; i++)
	{
		const QString& line = m_lines[m_top + i];
		Style st = Style::Normal;
		const QString low = line.toLower();
		if (low.contains("fail") || low.contains("error"))
			st = Style::Red;
		else if (low.contains("complete") || low.contains("finish"))
			st = Style::Cyan;
		if (!m_needle.isEmpty() && low.contains(m_needle.toLower()))
			st = Style::Selected;
		c.put(area.x + 1, area.y + i, fit(line, w - 1), st);
	}
	if (total == 0)
		c.put(area.x + 2, area.y + 1, m_app.tr("The log is empty."), Style::Dim);

	if (bar)
	{
		const int th = qMax(1, m_rows * m_rows / total);
		const int tp = m_top * (m_rows - th) / qMax(1, total - m_rows);
		for (int i = 0; i < m_rows; i++)
			c.put(area.right() - 1, area.y + i, i >= tp && i < tp + th ? g.scrollThumb : g.scrollTrack, Style::Dim);
	}
	hits.add(area, Action::None, TagLogArea);
}

void LogView::search(bool forward)
{
	if (m_needle.isEmpty() || m_lines.isEmpty())
		return;
	const int n = m_lines.size();
	for (int k = 1; k <= n; k++)
	{
		const int i = ((forward ? m_top + k : m_top - k) % n + n) % n;
		if (m_lines[i].contains(m_needle, Qt::CaseInsensitive))
		{
			m_follow = false;
			m_top = i;
			return;
		}
	}
	m_app.toast(m_app.tr("No match for \"%1\"").arg(m_needle), true);
}

void LogView::perform(Action a)
{
	const int page = qMax(1, m_rows - 1);
	auto scroll = [&](int d) { m_follow = false; m_top += d; };
	switch (a)
	{
		case Action::Up: return scroll(-1);
		case Action::Down: return scroll(1);
		case Action::PageUp: return scroll(-page);
		case Action::PageDown: return scroll(page);
		case Action::Home:
		case Action::TopOfLog:
			m_follow = false;
			m_top = 0;
			return;
		case Action::End:
		case Action::BottomOfLog:
			m_follow = true;
			return;
		case Action::Follow:
			m_follow = !m_follow;
			return;
		case Action::Search:
			dialogs::prompt(m_app, m_app.tr("Search the log"), m_app.tr("Text"), m_needle, m_app.tr("Find"), [this](const QString& s) {
				m_needle = s.trimmed();
				m_top = qMax(0, m_top - 1);
				search(true);
			});
			return;
		case Action::SearchNext: return search(true);
		case Action::TransferLog:
			if (!m_transferUuid.isEmpty())
			{
				m_transferUuid.clear();
			}
			else if (const TransferInfo* t = m_app.store().current())
			{
				m_transferUuid = t->uuid;
				m_transferName = t->name;
			}
			else
			{
				m_app.toast(m_app.tr("Select a transfer on the Transfers tab first"), true);
				return;
			}
			m_lines.clear();
			m_follow = true;
			poll();
			return;
		case Action::Back:
			if (!m_transferUuid.isEmpty())
			{
				m_transferUuid.clear();
				m_lines.clear();
				poll();
			}
			return;
		default: return;
	}
}

void LogView::mouse(const Event& e, const Hit*)
{
	if (e.mouse == Event::WheelUp)
	{
		m_follow = false;
		m_top = qMax(0, m_top - 3);
	}
	else if (e.mouse == Event::WheelDown)
	{
		m_follow = false;
		m_top += 3;
	}
}

}
