#include "DetailsView.h"
#include "Format.h"
#include <QJsonArray>
#include <QTimer>

namespace tui
{

static const int Levels[] = { 0, 1, 4, 7 };

static QString priorityName(int p)
{
	switch (p)
	{
		case 0: return "Skip";
		case 1: return "Normal";
		case 4: return "High";
		case 7: return "Maximum";
		default: return QString::number(p);
	}
}

static Style priorityStyle(int p)
{
	switch (p)
	{
		case 0: return Style::Dim;
		case 4: return Style::Yellow;
		case 7: return Style::Red;
		default: return Style::Normal;
	}
}

DetailsView::DetailsView(App& app, const QString& uuid) : View(app), m_uuid(uuid)
{
	m_logNav.cursor = 1 << 20;
	for (const TransferInfo& t : app.store().transfers())
		if (t.uuid == uuid)
		{
			m_t = t;
			m_haveTransfer = true;
		}
}

QVector<BarItem> DetailsView::bar() const
{
	QVector<BarItem> items = View::bar();
	if (!hasFiles())
		for (int i = items.size() - 1; i >= 0; i--)
			if (items[i].action == Action::NextTab && false)
				items.removeAt(i);
	for (BarItem& i : items)
		if (i.action == Action::ApplyFiles)
			i.enabled = !m_pending.isEmpty();
	return items;
}

QString DetailsView::statusText() const
{
	QString s = " " + m_t.name;
	if (!m_pending.isEmpty())
		s += m_app.tr(" · %1 unapplied priority change(s)").arg(m_pending.size());
	return s;
}

void DetailsView::poll()
{
	const int gen = ++m_gen;
	const QString uuid = ApiClient::encode(m_uuid);

	m_app.api().get("/transfers/" + uuid, [this, gen, alive = m_alive](const ApiResult& r) {
		if (!*alive || gen < m_applied)
			return;
		m_applied = gen;
		if (r.status == 404)
		{
			m_app.toast(m_app.tr("The transfer no longer exists"), true);
			m_app.closeDetails();
			return;
		}
		if (r.ok())
		{
			m_t = TransferInfo::fromJson(r.json.toObject());
			m_haveTransfer = true;
		}
	});
	m_app.api().transferDetails(m_uuid, [this, gen, alive = m_alive](const ApiResult& r) {
		if (*alive && gen >= m_applied && r.ok())
			m_info = r.json.toObject();
	});
	m_app.api().get("/transfers/" + uuid + "/speed-data", [this, gen, alive = m_alive](const ApiResult& r) {
		if (!*alive || gen < m_applied || !r.ok())
			return;
		m_speeds.clear();
		for (const QJsonValue& v : r.json.toArray())
			m_speeds << SpeedPair::fromJson(v);
	});
	if (m_tab == Log)
		m_app.api().transferLog(m_uuid, [this, gen, alive = m_alive](const ApiResult& r) {
			if (!*alive || gen < m_applied || !r.ok())
				return;
			m_log = QString::fromUtf8(r.raw).split('\n');
			while (!m_log.isEmpty() && m_log.last().isEmpty())
				m_log.removeLast();
		});
}

void DetailsView::nextTab(int dir)
{
	QVector<Tab> tabs = { Info };
	if (hasFiles())
		tabs << Files;
	tabs << Log;
	int i = tabs.indexOf(m_tab);
	i = (i + dir + tabs.size()) % tabs.size();
	m_tab = tabs[i];
	if (m_tab == Log)
		poll();
}

int DetailsView::priorityOf(int index) const
{
	if (m_pending.contains(index))
		return m_pending[index];
	const QJsonObject f = m_info["files"].toArray().at(index).toObject();
	return f.contains("priority") ? f["priority"].toInt() : 1;
}

void DetailsView::setPriority(int index, int prio)
{
	const QJsonObject f = m_info["files"].toArray().at(index).toObject();
	const int orig = f.contains("priority") ? f["priority"].toInt() : 1;
	if (prio == orig)
		m_pending.remove(index);
	else
		m_pending[index] = prio;
}

void DetailsView::changePriority(int index, int delta)
{
	if (index < 0 || index >= m_info["files"].toArray().size())
		return;
	const int cur = priorityOf(index);
	int level = 1;
	for (int i = 0; i < 4; i++)
		if (Levels[i] <= cur)
			level = i;
	level = qBound(0, level + delta, 3);
	setPriority(index, Levels[level]);
}

void DetailsView::apply()
{
	if (m_pending.isEmpty())
		return;
	QJsonArray items;
	for (auto it = m_pending.begin(); it != m_pending.end(); ++it)
		items.append(QJsonObject{ { "index", it.key() }, { "priority", it.value() } });
	const int n = m_pending.size();
	m_app.api().setFilePriorities(m_uuid, items, m_app.reply(m_app.tr("Cannot set the priorities"), [this, n, alive = m_alive]() {
		if (!*alive)
			return;
		m_pending.clear();
		m_app.toast(m_app.tr("Applied %1 priority change(s)").arg(n));
		poll();
	}));
}

void DetailsView::perform(Action a)
{
	const int nfiles = m_info["files"].toArray().size();
	const int page = qMax(1, m_rows - 1);
	ListNav& nav = m_tab == Log ? m_logNav : m_files;
	if (m_tab == Log)
		nav.cursor = qMin(nav.cursor, qMax(0, int(m_log.size()) - m_rows));

	switch (a)
	{
		case Action::Back:
			m_app.closeDetails();
			return;
		case Action::NextTab: return nextTab(1);
		case Action::PrevTab: return nextTab(-1);
		case Action::Up: nav.move(-1); break;
		case Action::Down: nav.move(1); break;
		case Action::PageUp: nav.move(-page); break;
		case Action::PageDown: nav.move(page); break;
		case Action::Home: nav.cursor = 0; break;
		case Action::End: nav.cursor = 1 << 20; break;
		case Action::PrioUp: return changePriority(m_files.cursor, 1);
		case Action::PrioDown: return changePriority(m_files.cursor, -1);
		case Action::SkipFile:
			if (m_files.cursor < nfiles)
				setPriority(m_files.cursor, priorityOf(m_files.cursor) == 0 ? 1 : 0);
			return;
		case Action::SelectAllFiles:
			for (int i = 0; i < nfiles; i++)
				if (priorityOf(i) == 0)
					setPriority(i, 1);
			return;
		case Action::ApplyFiles: return apply();
		default: return;
	}
}

void DetailsView::mouse(const Event& e, const Hit* hit)
{
	ListNav& nav = m_tab == Log ? m_logNav : m_files;
	const int count = m_tab == Log ? m_log.size() : m_info["files"].toArray().size();
	if (e.mouse == Event::WheelUp || e.mouse == Event::WheelDown)
	{
		const int d = e.mouse == Event::WheelUp ? -3 : 3;
		if (m_tab == Log)
			nav.cursor = qBound(0, qMin(nav.cursor, qMax(0, count - m_rows)) + d, qMax(0, count - m_rows));
		else
			nav.cursor = qBound(0, nav.cursor + d, qMax(0, count - 1));
		return;
	}
	if (e.mouse != Event::Press || e.button != 1 || !hit)
		return;
	if (hit->tag == TagDetailsTab)
	{
		m_tab = Tab(hit->index);
		if (m_tab == Log)
			poll();
	}
	else if (hit->tag == TagFileRow)
	{
		m_files.cursor = hit->index;
		if (e.doubleClick)
			setPriority(hit->index, priorityOf(hit->index) == 0 ? 1 : 0);
	}
}

// ----------------------------------------------------------------- drawing

void DetailsView::draw(Canvas& c, HitMap& hits, const Rect& area)
{
	const Glyphs& g = Glyphs::get();
	c.fill({ area.x, area.y, area.w, 1 }, Style::Header);
	c.put(area.x + 1, area.y, fit(m_t.name.isEmpty() ? m_uuid : m_t.name, area.w - 2), Style::Header);

	struct T { Tab t; const char* name; };
	int x = area.x + 1;
	for (const T& tb : { T{ Info, "Info" }, T{ Files, "Files" }, T{ Log, "Log" } })
	{
		if (tb.t == Files && !hasFiles())
			continue;
		const QString label = QString(" %1 ").arg(m_app.tr(tb.name));
		c.put(x, area.y + 1, label, m_tab == tb.t ? Style::TabOn : Style::Dim);
		hits.add({ x, area.y + 1, displayWidth(label), 1 }, Action::None, TagDetailsTab, tb.t);
		x += displayWidth(label) + 2;
	}
	const Rect body = { area.x, area.y + 3, area.w, area.h - 3 };
	m_rows = body.h;
	switch (m_tab)
	{
		case Info: drawInfo(c, body); break;
		case Files: drawFiles(c, hits, body); break;
		case Log: drawLog(c, body); break;
	}
	Q_UNUSED(g);
}

static QString sparkline(const QVector<SpeedPair>& v, bool up, int width)
{
	const Glyphs& g = Glyphs::get();
	QVector<qint64> s;
	for (const SpeedPair& p : v)
		s << (up ? p.up : p.down);
	if (s.size() > width)
		s = s.mid(s.size() - width);
	qint64 max = 1;
	for (qint64 x : s)
		max = qMax(max, x);
	QString out;
	for (qint64 x : s)
		out += g.spark[qBound<qint64>(0, x * 7 / max, qint64(7))];
	return out;
}

void DetailsView::drawInfo(Canvas& c, const Rect& r)
{
	const Glyphs& g = Glyphs::get();
	int y = r.y;
	auto row = [&](const QString& label, const QString& value, Style st = Style::Normal) {
		if (y >= r.bottom())
			return;
		c.put(r.x + 1, y, fit(label, 12), Style::Dim);
		c.put(r.x + 14, y, fit(value, r.w - 15), st);
		y++;
	};

	Style stateSt = Style::Normal;
	switch (m_t.state)
	{
		case TransferInfo::Active: case TransferInfo::ForcedActive: stateSt = Style::Green; break;
		case TransferInfo::Paused: stateSt = Style::Yellow; break;
		case TransferInfo::Failed: stateSt = Style::Red; break;
		case TransferInfo::Completed: stateSt = Style::Cyan; break;
		default: break;
	}
	auto limit = [&](qint64 v) { return v > 0 ? formatSpeed(v) : m_app.tr("unlimited"); };

	row(m_app.tr("State"), TransferInfo::stateName(m_t.state), stateSt);
	row(m_app.tr("Message"), m_t.message, m_t.state == TransferInfo::Failed ? Style::Red : Style::Normal);
	row(m_app.tr("Engine"), m_t.cls + (m_t.upload ? m_app.tr(" (upload)") : QString()));
	row(m_app.tr("Path"), m_t.dataPath);
	row(m_app.tr("Source"), m_t.object);
	row(m_app.tr("Size"), QString("%1 of %2 (%3%)").arg(formatSize(m_t.done), formatSize(m_t.total)).arg(m_t.percent()));
	row(m_app.tr("Running"), formatDuration(m_t.timeRunning));
	row(m_app.tr("Limits"), QString("%1 %2    %3 %4").arg(g.down, limit(m_t.userSpeedLimits.down), g.up, limit(m_t.userSpeedLimits.up)));
	if (!m_t.comment.isEmpty())
		row(m_app.tr("Comment"), m_t.comment);
	if (m_info.contains("ratio"))
	{
		row(m_app.tr("Ratio"), m_info["ratio"].toString());
		row(m_app.tr("Total"), QString("%1 %2    %3 %4").arg(g.down, m_info["totalDownload"].toString(), g.up, m_info["totalUpload"].toString()));
	}
	if (m_info.contains("magnetLink"))
		row(m_app.tr("Magnet"), m_info["magnetLink"].toString());

	y++;
	const int sw = qMax(10, r.w - 40);
	if (y + 1 < r.bottom())
	{
		c.put(r.x + 1, y, fit(m_app.tr("Download"), 12), Style::Dim);
		c.put(r.x + 14, y, fit(sparkline(m_speeds, false, sw), sw), Style::Green);
		c.put(r.x + 15 + sw, y, formatSpeed(m_t.speeds.down), Style::Normal);
		y++;
		c.put(r.x + 1, y, fit(m_app.tr("Upload"), 12), Style::Dim);
		c.put(r.x + 14, y, fit(sparkline(m_speeds, true, sw), sw), Style::Cyan);
		c.put(r.x + 15 + sw, y, formatSpeed(m_t.speeds.up), Style::Normal);
	}
}

void DetailsView::drawFiles(Canvas& c, HitMap& hits, const Rect& r)
{
	const QJsonArray files = m_info["files"].toArray();
	const int rows = r.h - 1;
	m_files.clamp(files.size(), rows);

	const int nameW = qMax(10, r.w - 2 - 10 - 7 - 11 - 4);
	c.put(r.x, r.y, fit("  " + m_app.tr("File"), nameW + 2) + fitRight(m_app.tr("Size"), 10) + fitRight(m_app.tr("Done"), 7) + "  " + fit(m_app.tr("Priority"), 9), Style::Header);
	c.fill({ r.x + nameW + 2 + 10 + 7 + 2 + 9, r.y, qMax(0, r.w - (nameW + 2 + 10 + 7 + 2 + 9)), 1 }, Style::Header);

	for (int i = 0; i < rows && m_files.scroll + i < files.size(); i++)
	{
		const int idx = m_files.scroll + i;
		const QJsonObject f = files[idx].toObject();
		const qint64 size = qint64(f["size"].toDouble()), done = qint64(f["done"].toDouble());
		const int pct = size > 0 ? int(done * 100 / size) : 100;
		const int prio = priorityOf(idx);
		const bool pend = m_pending.contains(idx);
		const bool cur = idx == m_files.cursor;
		const int y = r.y + 1 + i;
		const QString prefix = QString(cur ? Glyphs::get().cursor : " ") + " ";
		const QString line = prefix + fit(f["name"].toString(), nameW) + fitRight(formatSize(size), 10) + fitRight(QString("%1%").arg(pct), 7) + "  ";
		const QString pn = fit(priorityName(prio) + (pend ? "*" : ""), 9);
		if (cur)
			c.put(r.x, y, fit(line + pn, r.w), Style::Selected);
		else
		{
			c.put(r.x, y, line, prio == 0 ? Style::Dim : Style::Normal);
			c.put(r.x + displayWidth(line), y, pn, priorityStyle(prio));
		}
		hits.add({ r.x, y, r.w, 1 }, Action::SelectRow, TagFileRow, idx);
	}
	if (files.isEmpty())
		c.put(r.x + 2, r.y + 2, m_app.tr("No file list yet."), Style::Dim);
}

void DetailsView::drawLog(Canvas& c, const Rect& r)
{
	const int total = m_log.size();
	const int rows = r.h;
	// the cursor is the top line; a large value follows the end of the log
	m_logNav.scroll = qBound(0, m_logNav.cursor, qMax(0, total - rows));
	for (int i = 0; i < rows && m_logNav.scroll + i < total; i++)
		c.put(r.x + 1, r.y + i, fit(m_log[m_logNav.scroll + i], r.w - 2), Style::Normal);
	if (total == 0)
		c.put(r.x + 2, r.y + 1, m_app.tr("This transfer has no log messages."), Style::Dim);
}

}
