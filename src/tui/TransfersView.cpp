#include "TransfersView.h"
#include "Dialogs.h"
#include "Format.h"
#include <QJsonArray>

namespace tui
{

static Style stateStyle(TransferInfo::State s)
{
	switch (s)
	{
		case TransferInfo::Active:
		case TransferInfo::ForcedActive: return Style::Green;
		case TransferInfo::Paused: return Style::Yellow;
		case TransferInfo::Completed: return Style::Cyan;
		case TransferInfo::Failed: return Style::Red;
		default: return Style::Dim;
	}
}

static QString progressBar(int percent, int width)
{
	const Glyphs& g = Glyphs::get();
	const int filled = qBound(0, percent * width / 100, width);
	return repeat(g.barFull, filled) + repeat(g.barEmpty, width - filled);
}

TransfersView::Columns TransfersView::columns(int width) const
{
	Columns col;
	auto need = [&](const Columns& k) {
		return 2 + 1 + k.state + 1 + k.progress + 1 + k.down + (k.showSize ? 1 + k.size : 0) + (k.showEta ? 1 + k.eta : 0);
	};
	col.name = width - need(col) - 1;
	if (col.name < 14)
	{
		col.showEta = false;
		col.name = width - need(col) - 1;
	}
	if (col.name < 14)
	{
		col.showSize = false;
		col.name = width - need(col) - 1;
	}
	col.name = qMax(4, col.name);
	return col;
}

// ----------------------------------------------------------------- drawing

void TransfersView::draw(Canvas& c, HitMap& hits, const Rect& area)
{
	Store& st = m_app.store();
	m_compact = area.w < 100;

	Rect tbox;
	if (!m_compact)
	{
		m_queueBox = { area.x, area.y, 24, area.h };
		tbox = { area.x + 24, area.y, area.w - 24, area.h };
		drawQueues(c, hits, m_queueBox);
	}
	else
	{
		m_queueBox = {};
		const Glyphs& g = Glyphs::get();
		const QueueInfo* q = st.currentQueue();
		const QString label = QString("%1 %2 %3").arg(g.left, q ? q->name : QString("-"), g.right);
		c.put(area.x + 1, area.y, "Queue ", Style::Dim);
		c.put(area.x + 7, area.y, label, Style::Hl);
		hits.add({ area.x + 7, area.y, 2, 1 }, Action::Left, TagQueueSwitch);
		hits.add({ area.x + 7 + displayWidth(label) - 2, area.y, 2, 1 }, Action::Right, TagQueueSwitch);
		tbox = { area.x, area.y + 1, area.w, area.h - 1 };
	}
	drawTransfers(c, hits, tbox);
}

void TransfersView::drawQueues(Canvas& c, HitMap& hits, const Rect& box)
{
	Store& st = m_app.store();
	const Glyphs& g = Glyphs::get();
	c.box(box, m_focusQueues ? Style::BorderFocus : Style::Border, m_app.tr("Queues"), m_focusQueues ? Style::Hl : Style::Dim);
	hits.add(box, Action::FocusPane, TagPane, 0);

	const int inner = box.w - 2;
	const bool info = box.h >= 14;
	const int listRows = box.h - 2 - (info ? 6 : 0);

	for (int i = 0; i < st.queues().size() && i < listRows; i++)
	{
		const QueueInfo& q = st.queues()[i];
		const bool cur = i == st.queueIndex();
		const int count = st.queueCount(q.uuid);
		const QString num = count >= 0 ? QString::number(count) : QString();
		const QString text = QString(" %1%2%3 ").arg(cur ? g.cursor : " ", fit(q.name, inner - 3 - 4), fitRight(num, 4));
		Style s = Style::Normal;
		if (i == m_dropQueue)
			s = Style::MenuSel;
		else if (cur)
			s = m_focusQueues ? Style::Selected : Style::SelectedDim;
		c.put(box.x + 1, box.y + 1 + i, fit(text, inner), s);
		hits.add({ box.x + 1, box.y + 1 + i, inner, 1 }, Action::SelectRow, TagQueueRow, i);
	}

	if (info)
	{
		if (const QueueInfo* q = st.currentQueue())
		{
			const int y = box.bottom() - 6;
			auto limit = [&](qint64 v) { return v > 0 ? formatSpeed(v) : g.inf; };
			c.put(box.x + 1, y, fit(" " + q->name, inner), Style::Hl);
			c.put(box.x + 1, y + 1, fit(" Dir  " + q->defaultDirectory, inner), Style::Dim);
			c.put(box.x + 1, y + 2, fit(" Move " + (q->moveDirectory.isEmpty() ? "-" : q->moveDirectory), inner), Style::Dim);
			c.put(box.x + 1, y + 3, fit(QString(" Lim  %1 %2  %3 %4").arg(g.down, limit(q->speedLimits.down), g.up, limit(q->speedLimits.up)), inner), Style::Dim);
		}
	}
}

void TransfersView::ensureVisible()
{
	Store& st = m_app.store();
	const int total = st.visible().size();
	m_scroll = qBound(0, m_scroll, qMax(0, total - m_rows));
	if (!m_follow || m_rows <= 0)
		return;
	if (st.cursor() < m_scroll)
		m_scroll = st.cursor();
	else if (st.cursor() >= m_scroll + m_rows)
		m_scroll = st.cursor() - m_rows + 1;
}

void TransfersView::drawTransfers(Canvas& c, HitMap& hits, const Rect& box)
{
	Store& st = m_app.store();
	const Glyphs& g = Glyphs::get();

	QString title = m_app.tr("Transfers");
	if (const QueueInfo* q = st.currentQueue())
		title += " · " + q->name;
	if (!st.filter().isEmpty())
		title += m_app.tr(" (filter: %1)").arg(st.filter());
	c.box(box, m_focusQueues ? Style::Border : Style::BorderFocus, title, m_focusQueues ? Style::Dim : Style::Hl);
	hits.add(box, Action::FocusPane, TagPane, 1);

	const Rect in = box.inset(1, 1);
	if (in.h < 3)
		return;

	m_stripVisible = in.h >= 16;
	const int stripH = m_stripVisible ? 7 : 0;
	m_rows = in.h - 1 - stripH;
	m_listRect = { in.x, in.y + 1, in.w, m_rows };

	const QVector<int> vis = st.visible();
	const bool needScroll = vis.size() > m_rows;
	const int w = in.w - (needScroll ? 1 : 0);
	ensureVisible();

	// header
	const Columns col = columns(w);
	auto header = [&]() {
		QString h = "  " + fit(m_app.tr("Name"), col.name) + " " + fit(m_app.tr("State"), col.state) + " " + fit(m_app.tr("Progress"), col.progress);
		if (col.showSize)
			h += " " + fitRight(m_app.tr("Size"), col.size);
		h += " " + fitRight(m_app.tr("Down"), col.down);
		if (col.showEta)
			h += " " + fitRight(m_app.tr("ETA"), col.eta);
		return h;
	};
	c.put(in.x, in.y, fit(header(), in.w), Style::Header);

	if (vis.isEmpty())
	{
		const QString msg = st.queues().isEmpty() ? m_app.tr("No queues yet. Switch to the queue list and press N.")
			: st.filter().isEmpty() ? m_app.tr("No transfers in this queue. Press N to add one.") : m_app.tr("Nothing matches the filter.");
		c.put(in.x + 2, in.y + 2, fit(msg, in.w - 4), Style::Dim);
	}

	for (int r = 0; r < m_rows && m_scroll + r < vis.size(); r++)
	{
		const int vi = m_scroll + r;
		const TransferInfo& t = st.transfers()[vis[vi]];
		const int y = m_listRect.y + r;
		const bool cur = vi == st.cursor();
		const bool marked = st.marks().contains(t.uuid);

		const QString lead = QString(cur ? g.cursor : " ") + (marked ? g.marked : " ");
		const QString name = fit(t.name, col.name);
		const QString state = fit(TransferInfo::stateName(t.state), col.state);
		const QString prog = progressBar(t.percent(), 10) + fitRight(QString("%1%").arg(t.percent()), 5);
		const QString size = fitRight(formatSize(t.total), col.size);
		const bool showSpeed = t.isActive();
		const QString down = fitRight(showSpeed ? formatSpeed(t.upload ? t.speeds.up : t.speeds.down) : formatSpeed(0), col.down);
		const QString eta = fitRight(showSpeed ? formatEta(t.total, t.done, t.upload ? t.speeds.up : t.speeds.down) : QString::fromUtf8("—"), col.eta);

		if (cur)
		{
			QString line = lead + name + " " + state + " " + prog;
			if (col.showSize) line += " " + size;
			line += " " + down;
			if (col.showEta) line += " " + eta;
			c.put(in.x, y, fit(line, w), m_focusQueues ? Style::SelectedDim : Style::Selected);
		}
		else
		{
			int x = in.x;
			c.put(x, y, lead.left(1) + " ", Style::Normal);
			if (marked)
				c.put(x + 1, y, g.marked, Style::Mark);
			x += 2;
			c.put(x, y, name, Style::Normal); x += col.name + 1;
			c.put(x, y, state, stateStyle(t.state)); x += col.state + 1;
			c.put(x, y, prog, t.state == TransferInfo::Failed ? Style::Red : Style::Normal); x += col.progress + 1;
			if (col.showSize) { c.put(x, y, size, Style::Normal); x += col.size + 1; }
			c.put(x, y, down, Style::Normal); x += col.down + 1;
			if (col.showEta)
				c.put(x, y, eta, Style::Dim);
		}
		hits.add({ in.x, y, w, 1 }, Action::SelectRow, TagTransferRow, vi);

		if (m_press.active && m_dropRow == vi)
			c.put(in.x, y, g.right + g.right, Style::Hl);
	}

	// scrollbar
	m_scrollbar = {};
	if (needScroll && m_rows > 0)
	{
		m_scrollbar = { in.right() - 1, m_listRect.y, 1, m_rows };
		const int th = qMax(1, m_rows * m_rows / int(vis.size()));
		const int tp = m_scroll * (m_rows - th) / qMax(1, int(vis.size()) - m_rows);
		for (int i = 0; i < m_rows; i++)
			c.put(m_scrollbar.x, m_scrollbar.y + i, i >= tp && i < tp + th ? g.scrollThumb : g.scrollTrack, Style::Dim);
		hits.add(m_scrollbar, Action::ScrollUp, TagScroll);
	}

	if (m_stripVisible)
		drawDetails(c, { in.x, in.bottom() - stripH, in.w, stripH });
}

static QString spark(const QVector<SpeedPair>& v, bool up, int width)
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

void TransfersView::drawDetails(Canvas& c, const Rect& r)
{
	const Glyphs& g = Glyphs::get();
	c.put(r.x, r.y, fit(g.h + " " + m_app.tr("Details") + " " + repeat(g.h, r.w), r.w), Style::Dim);
	const TransferInfo* t = m_app.store().current();
	if (!t)
		return;

	auto row = [&](int i, const QString& label, const QString& value, Style vs = Style::Normal) {
		c.put(r.x + 1, r.y + 1 + i, fit(label, 8), Style::Dim);
		c.put(r.x + 9, r.y + 1 + i, fit(value, r.w - 10), vs);
	};
	row(0, m_app.tr("Name"), t->name);
	row(1, m_app.tr("Path"), t->dataPath);

	QString cls = t->cls;
	const QJsonObject d = m_infoUuid == t->uuid ? m_info : QJsonObject();
	if (d.contains("ratio"))
		cls += m_app.tr("     Ratio %1").arg(d["ratio"].toString());
	if (d.contains("totalDownload"))
		cls += QString("   %1 %2  %3 %4").arg(g.down, d["totalDownload"].toString(), g.up, d["totalUpload"].toString());
	cls += m_app.tr("   Running %1").arg(formatDuration(t->timeRunning));
	row(2, m_app.tr("Class"), cls);

	const QVector<SpeedPair> sp = m_infoUuid == t->uuid ? m_speeds : QVector<SpeedPair>();
	const int sw = qMax(8, (r.w - 40) / 2);
	c.put(r.x + 1, r.y + 4, fit(m_app.tr("Speed"), 8), Style::Dim);
	int x = r.x + 9;
	c.put(x, r.y + 4, g.down + " ", Style::Normal);
	c.put(x + 2, r.y + 4, fit(spark(sp, false, sw), sw), Style::Green);
	const QString dtxt = " " + formatSpeed(t->speeds.down) + "    " + g.up + " ";
	c.put(x + 2 + sw, r.y + 4, dtxt, Style::Normal);
	const int ux = x + 2 + sw + displayWidth(dtxt);
	c.put(ux, r.y + 4, fit(spark(sp, true, sw), sw), Style::Cyan);
	c.put(ux + sw, r.y + 4, " " + formatSpeed(t->speeds.up), Style::Normal);

	auto lim = [&](qint64 v) { return v > 0 ? formatSpeed(v) : m_app.tr("unlimited"); };
	row(3, m_app.tr("Limits"), QString("%1 %2    %3 %4").arg(g.down, lim(t->userSpeedLimits.down), g.up, lim(t->userSpeedLimits.up)));
	row(4, m_app.tr("Message"), t->message, t->state == TransferInfo::Failed ? Style::Red : Style::Normal);
	if (!t->comment.isEmpty())
		row(5, m_app.tr("Comment"), t->comment);
}

// ------------------------------------------------------------------ polling

void TransfersView::poll()
{
	const TransferInfo* t = m_app.store().current();
	if (!t || !m_stripVisible)
		return;
	const QString uuid = t->uuid;
	m_app.api().transferDetails(uuid, [this, uuid](const ApiResult& r) {
		if (r.ok() && m_app.store().current() && m_app.store().current()->uuid == uuid)
		{
			if (m_infoUuid != uuid)
				m_speeds.clear();
			m_info = r.json.toObject();
			m_infoUuid = uuid;
		}
	});
	m_app.api().get("/transfers/" + ApiClient::encode(uuid) + "/speed-data", [this, uuid](const ApiResult& r) {
		if (!r.ok())
			return;
		QVector<SpeedPair> v;
		for (const QJsonValue& x : r.json.toArray())
			v << SpeedPair::fromJson(x);
		m_speeds = v;
		if (m_infoUuid != uuid)
		{
			m_infoUuid = uuid;
			m_info = QJsonObject();
		}
	});
}

// ---------------------------------------------------------------- actions

QVector<BarItem> TransfersView::bar() const
{
	QVector<BarItem> items = Keymap::bar(context());
	const Store& st = m_app.store();
	const Availability av = st.availability();
	for (BarItem& i : items)
	{
		switch (i.action)
		{
			case Action::Resume: i.enabled = av.resume; break;
			case Action::Force: i.enabled = av.force; break;
			case Action::Pause: i.enabled = av.pause; break;
			case Action::DeleteTransfers: i.enabled = av.del; break;
			case Action::EditTransfer: i.enabled = av.edit; break;
			case Action::MoveMenu: i.enabled = av.moveUp || av.moveDown; break;
			case Action::SendToQueue: i.enabled = av.any && st.queues().size() > 1; break;
			case Action::LimitTransfer:
			case Action::ToggleMark: i.enabled = av.any; break;
			case Action::Details: i.enabled = st.current() != nullptr; break;
			case Action::EditQueue:
			case Action::DeleteQueue:
			case Action::LimitQueue: i.enabled = st.currentQueue() != nullptr; break;
			case Action::QueueMoveUp: i.enabled = st.queueIndex() > 0; break;
			case Action::QueueMoveDown: i.enabled = st.queueIndex() < st.queues().size() - 1; break;
			default: break;
		}
	}
	return items;
}

void TransfersView::setState(const QString& state, TransferInfo::State local)
{
	Store& st = m_app.store();
	const QStringList t = st.targets();
	if (t.isEmpty())
		return;
	st.setStateLocally(t, local);
	m_app.api().patchTransfers(t, QJsonObject{ { "state", state } }, m_app.reply(m_app.tr("Cannot change the state")));
}

void TransfersView::moveSelected(const QString& direction)
{
	Store& st = m_app.store();
	const QStringList t = st.targets();
	if (t.isEmpty())
		return;
	m_follow = true;
	m_app.api().moveTransfers(st.currentQueueUuid(), t, direction, m_app.reply(m_app.tr("Cannot move")));
}

void TransfersView::moveQueue(const QString& direction)
{
	const QueueInfo* q = m_app.store().currentQueue();
	if (!q)
		return;
	m_app.api().moveQueue(q->uuid, direction, m_app.reply(m_app.tr("Cannot move the queue"), [this]() { m_app.refreshQueues(); }));
}

void TransfersView::reorderQueue(int from, int to)
{
	const QVector<QueueInfo>& qs = m_app.store().queues();
	if (from == to || from < 0 || to < 0 || from >= qs.size() || to >= qs.size())
		return;
	const QString uuid = qs[from].uuid;
	const QString dir = to < from ? "up" : "down";
	// the API moves one step at a time
	auto step = std::make_shared<std::function<void(int)>>();
	*step = [this, uuid, dir, step](int left) {
		if (left == 0)
		{
			m_app.refreshQueues();
			return;
		}
		m_app.api().moveQueue(uuid, dir, [this, step, left](const ApiResult& r) {
			if (!r.ok())
			{
				m_app.toast(m_app.tr("Cannot move the queue: %1").arg(r.describe()), true);
				m_app.refreshQueues();
				return;
			}
			(*step)(left - 1);
		});
	};
	(*step)(qAbs(to - from));
}

void TransfersView::navigate(int delta, bool absolute)
{
	Store& st = m_app.store();
	m_follow = true;
	if (m_focusQueues)
	{
		if (absolute)
			st.selectQueue(delta < 0 ? 0 : st.queues().size() - 1);
		else
			st.moveQueueCursor(delta);
		m_scroll = 0;
		m_app.refreshSoon();
		return;
	}
	if (absolute)
		delta < 0 ? st.cursorHome() : st.cursorEnd();
	else
		st.moveCursor(delta);
}

void TransfersView::perform(Action a)
{
	Store& st = m_app.store();
	const int page = qMax(1, m_rows - 1);

	switch (a)
	{
		case Action::Up: return navigate(-1);
		case Action::Down: return navigate(1);
		case Action::PageUp: return navigate(-page);
		case Action::PageDown: return navigate(page);
		case Action::Home: return navigate(-1, true);
		case Action::End: return navigate(1, true);

		case Action::Left:
			if (m_compact)
			{
				st.moveQueueCursor(-1);
				m_app.refreshSoon();
			}
			else
				m_focusQueues = true;
			return;
		case Action::Right:
			if (m_compact)
			{
				st.moveQueueCursor(1);
				m_app.refreshSoon();
			}
			else
				m_focusQueues = false;
			return;
		case Action::FocusNext:
		case Action::FocusPrev:
		case Action::FocusPane:
			if (!m_compact)
				m_focusQueues = !m_focusQueues;
			return;
		case Action::Back:
			if (!st.filter().isEmpty())
				st.setFilter(QString());
			else if (!st.marks().isEmpty())
				st.clearMarks();
			return;
		case Action::OpenQueue:
			m_focusQueues = false;
			return;

		case Action::Resume: return setState("Waiting", TransferInfo::Waiting);
		case Action::Force: return setState("ForcedActive", TransferInfo::ForcedActive);
		case Action::Pause: return setState("Paused", TransferInfo::Paused);
		case Action::NewTransfer: return dialogs::newTransfer(m_app);
		case Action::DeleteTransfers:
		{
			QStringList names;
			for (const TransferInfo* t : st.targetTransfers())
				names << t->name;
			if (!names.isEmpty())
				dialogs::deleteTransfers(m_app, st.targets(), names);
			return;
		}
		case Action::EditTransfer:
			if (const TransferInfo* t = st.current())
				dialogs::editTransfer(m_app, *t);
			return;
		case Action::LimitTransfer:
		{
			const auto t = st.targetTransfers();
			if (!t.isEmpty())
				dialogs::limitTransfers(m_app, st.targets(), t.size() == 1 ? t[0]->userSpeedLimits : SpeedPair());
			return;
		}
		case Action::MoveMenu:
		{
			const int y = m_listRect.y + qBound(0, st.cursor() - m_scroll, qMax(0, m_rows - 1));
			QVector<MenuItem> items;
			items << MenuItem{ m_app.tr("To the top"), "T", true, false, [this]() { moveSelected("top"); } }
				<< MenuItem{ m_app.tr("Up"), "U", true, false, [this]() { moveSelected("up"); } }
				<< MenuItem{ m_app.tr("Down"), "D", true, false, [this]() { moveSelected("down"); } }
				<< MenuItem{ m_app.tr("To the bottom"), "B", true, false, [this]() { moveSelected("bottom"); } };
			m_app.push(std::make_unique<MenuDialog>(items, m_listRect.x + 4, y, m_app.tr("Move")));
			return;
		}
		case Action::MoveUp: return moveSelected("up");
		case Action::MoveDown: return moveSelected("down");
		case Action::MoveTop: return moveSelected("top");
		case Action::MoveBottom: return moveSelected("bottom");
		case Action::SendToQueue:
			if (!st.targets().isEmpty())
				dialogs::sendToQueue(m_app, st.targets(), m_listRect.x + 4, m_listRect.y + 2);
			return;
		case Action::ToggleMark: return st.toggleMark();
		case Action::MarkAll: return st.markAll();
		case Action::Details:
			if (const TransferInfo* t = st.current())
				m_app.openDetails(t->uuid);
			return;

		case Action::NewQueue: return dialogs::queueEditor(m_app, nullptr);
		case Action::EditQueue:
		case Action::LimitQueue:
			if (const QueueInfo* q = st.currentQueue())
				dialogs::queueEditor(m_app, q);
			return;
		case Action::DeleteQueue:
			if (const QueueInfo* q = st.currentQueue())
				dialogs::deleteQueue(m_app, *q);
			return;
		case Action::QueueMoveUp: return moveQueue("up");
		case Action::QueueMoveDown: return moveQueue("down");
		default: return;
	}
}

// ------------------------------------------------------------------- mouse

void TransfersView::transferMenu(int x, int y)
{
	const Store& st = m_app.store();
	const Availability av = st.availability();
	auto act = [this](Action a) { return [this, a]() { perform(a); }; };
	QVector<MenuItem> items;
	items << MenuItem{ m_app.tr("Resume"), "R", av.resume, false, act(Action::Resume) }
		<< MenuItem{ m_app.tr("Force resume"), "F", av.force, false, act(Action::Force) }
		<< MenuItem{ m_app.tr("Pause"), "P", av.pause, false, act(Action::Pause) }
		<< MenuItem::sep()
		<< MenuItem{ m_app.tr("Delete…"), "D", av.del, false, act(Action::DeleteTransfers) }
		<< MenuItem::sep()
		<< MenuItem{ m_app.tr("Move to the top"), "", av.moveUp, false, [this]() { moveSelected("top"); } }
		<< MenuItem{ m_app.tr("Move up"), "[", av.moveUp, false, act(Action::MoveUp) }
		<< MenuItem{ m_app.tr("Move down"), "]", av.moveDown, false, act(Action::MoveDown) }
		<< MenuItem{ m_app.tr("Move to the bottom"), "", av.moveDown, false, [this]() { moveSelected("bottom"); } }
		<< MenuItem{ m_app.tr("Send to queue…"), "S", av.any && st.queues().size() > 1, false, act(Action::SendToQueue) }
		<< MenuItem::sep()
		<< MenuItem{ m_app.tr("Details"), "", st.current() != nullptr, false, act(Action::Details) }
		<< MenuItem{ m_app.tr("Properties…"), "E", av.edit, false, act(Action::EditTransfer) }
		<< MenuItem{ m_app.tr("Speed limit…"), "L", av.any, false, act(Action::LimitTransfer) };
	m_app.push(std::make_unique<MenuDialog>(items, x, y));
}

void TransfersView::queueMenu(int x, int y)
{
	const Store& st = m_app.store();
	auto act = [this](Action a) { return [this, a]() { perform(a); }; };
	QVector<MenuItem> items;
	items << MenuItem{ m_app.tr("New queue"), "N", true, false, act(Action::NewQueue) }
		<< MenuItem{ m_app.tr("Edit queue"), "E", st.currentQueue() != nullptr, false, act(Action::EditQueue) }
		<< MenuItem{ m_app.tr("Move up"), "[", st.queueIndex() > 0, false, act(Action::QueueMoveUp) }
		<< MenuItem{ m_app.tr("Move down"), "]", st.queueIndex() < st.queues().size() - 1, false, act(Action::QueueMoveDown) }
		<< MenuItem::sep()
		<< MenuItem{ m_app.tr("Delete queue…"), "D", st.currentQueue() != nullptr, false, act(Action::DeleteQueue) };
	m_app.push(std::make_unique<MenuDialog>(items, x, y));
}

QStringList TransfersView::dragUuids() const
{
	const Store& st = m_app.store();
	// dragging a marked transfer takes all marked ones along
	if (st.marks().contains(m_press.uuid))
		return st.targets();
	return { m_press.uuid };
}

void TransfersView::drop()
{
	Store& st = m_app.store();
	if (m_press.tag == TagTransferRow)
	{
		const QStringList uuids = dragUuids();
		if (m_dropQueue >= 0 && m_dropQueue < st.queues().size())
		{
			if (st.queues()[m_dropQueue].uuid != st.currentQueueUuid())
			{
				st.clearMarks();
				m_app.api().patchTransfers(uuids, QJsonObject{ { "queue", st.queues()[m_dropQueue].uuid } }, m_app.reply(m_app.tr("Cannot send to the queue")));
			}
		}
		else if (m_dropRow >= 0)
		{
			const QVector<int> vis = st.visible();
			QString before;
			if (m_dropRow < vis.size())
				before = st.transfers()[vis[m_dropRow]].uuid;
			if (!uuids.contains(before))
				m_app.api().moveTransfersBefore(st.currentQueueUuid(), uuids, before, m_app.reply(m_app.tr("Cannot move")));
		}
	}
	else if (m_press.tag == TagQueueRow && m_dropQueue >= 0)
	{
		reorderQueue(m_press.index, m_dropQueue);
	}
}

void TransfersView::mouse(const Event& e, const Hit* hit)
{
	Store& st = m_app.store();
	const int wheelStep = 3;

	switch (e.mouse)
	{
		case Event::WheelUp:
		case Event::WheelDown:
		{
			const int dir = e.mouse == Event::WheelUp ? -1 : 1;
			if (m_queueBox.contains(e.x, e.y))
			{
				st.moveQueueCursor(dir);
				m_app.refreshSoon();
			}
			else
			{
				m_follow = false;
				m_scroll += dir * wheelStep;
			}
			return;
		}

		case Event::Press:
		{
			m_press = Press();
			if (!hit)
				return;

			if (hit->tag == TagQueueSwitch)
				return perform(hit->action);

			if (hit->tag == TagScroll && e.button == 1)
			{
				const int total = st.visible().size();
				m_follow = false;
				m_scroll = (e.y - m_scrollbar.y) * qMax(0, total - m_rows) / qMax(1, m_scrollbar.h - 1);
				m_press.down = true;
				m_press.tag = TagScroll;
				return;
			}

			if (hit->tag == TagPane)
			{
				m_focusQueues = hit->index == 0;
				return;
			}

			if (hit->tag == TagTransferRow)
			{
				m_focusQueues = false;
				m_follow = true;
				const int row = hit->index;
				if (e.button == 3)
				{
					if (!st.marks().isEmpty() && st.marks().contains(st.transfers()[st.visible()[row]].uuid))
						; // keep the marked set
					else
						st.setCursor(row);
					transferMenu(e.x, e.y);
					return;
				}
				if (e.button != 1)
					return;
				if (e.ctrl)
				{
					st.toggleMarkAt(row);
					st.setCursor(row);
				}
				else if (e.shift)
				{
					st.markRange(m_anchor, row);
					st.setCursor(row);
				}
				else
				{
					st.setCursor(row);
					m_anchor = row;
				}
				if (e.doubleClick && !e.ctrl && !e.shift)
					return perform(Action::Details);
				m_press = { true, false, TagTransferRow, row, e.x, e.y, st.transfers()[st.visible()[row]].uuid };
				return;
			}

			if (hit->tag == TagQueueRow)
			{
				m_focusQueues = true;
				const int qi = hit->index;
				if (e.button == 1 || e.button == 3)
				{
					const bool changed = qi != st.queueIndex();
					st.selectQueue(qi);
					m_scroll = 0;
					if (changed)
						m_app.refreshSoon();
				}
				if (e.button == 3)
					return queueMenu(e.x, e.y);
				if (e.button != 1)
					return;
				if (e.doubleClick)
					return perform(Action::EditQueue);
				m_press = { true, false, TagQueueRow, qi, e.x, e.y, QString() };
			}
			return;
		}

		case Event::Drag:
		{
			if (!m_press.down)
				return;
			if (m_press.tag == TagScroll)
			{
				const int total = st.visible().size();
				m_scroll = qBound(0, (e.y - m_scrollbar.y) * qMax(0, total - m_rows) / qMax(1, m_scrollbar.h - 1), qMax(0, total - m_rows));
				return;
			}
			if (!m_press.active && (e.x != m_press.x || e.y != m_press.y))
				m_press.active = true;
			if (!m_press.active)
				return;

			m_dropRow = m_dropQueue = -1;
			if (hit && hit->tag == TagTransferRow && m_press.tag == TagTransferRow)
				m_dropRow = hit->index;
			else if (hit && hit->tag == TagQueueRow)
				m_dropQueue = hit->index;
			else if (m_press.tag == TagTransferRow && m_listRect.contains(e.x, e.y))
				m_dropRow = st.visible().size();
			// scroll while dragging near the edges
			if (m_press.tag == TagTransferRow && m_rows > 2)
			{
				if (e.y <= m_listRect.y) { m_follow = false; m_scroll = qMax(0, m_scroll - 1); }
				else if (e.y >= m_listRect.bottom() - 1) { m_follow = false; m_scroll++; }
			}
			return;
		}

		case Event::Release:
		{
			const bool wasActive = m_press.active;
			if (wasActive)
				drop();
			m_press = Press();
			m_dropRow = m_dropQueue = -1;
			Q_UNUSED(wasActive);
			return;
		}
	}
}

}
