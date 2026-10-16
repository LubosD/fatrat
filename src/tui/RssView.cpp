#include "RssView.h"
#include "Dialogs.h"
#include "Format.h"
#include <QJsonArray>

namespace tui
{

static const QStringList TvsValues = { "none", "season", "episode", "date" };

void RssView::poll()
{
	if (!m_dirty && ++m_ticks % 5 != 0)
		return;
	m_dirty = false;
	m_app.api().rssFeeds([this](const ApiResult& r) {
		if (!r.ok())
			return;
		m_feeds.clear();
		for (const QJsonValue& v : r.json.toArray())
		{
			const QJsonObject o = v.toObject();
			m_feeds.push_back({ o["id"].toString(), o["name"].toString(), o["url"].toString() });
		}
		m_loaded = true;
	});
	m_app.api().rssRegexps([this](const ApiResult& r) {
		if (!r.ok())
			return;
		m_ruleList.clear();
		for (const QJsonValue& v : r.json.toArray())
		{
			const QJsonObject o = v.toObject();
			RssRule x;
			x.id = o["id"].toString();
			x.feedId = o["feedId"].toString();
			x.expression = o["expression"].toString();
			x.queue = o["queue"].toString();
			x.target = o["target"].toString();
			x.tvs = o["tvs"].toString();
			x.from = o["from"].toString();
			x.to = o["to"].toString();
			x.linkRegexp = o["linkRegexp"].toString();
			x.addPaused = o["addPaused"].toBool();
			x.includeRepacks = o["includeRepacks"].toBool();
			x.includeTrailers = o["includeTrailers"].toBool();
			x.excludeManuals = o["excludeManuals"].toBool();
			m_ruleList.push_back(x);
		}
	});
}

const RssFeed* RssView::currentFeed() const
{
	return m_feedNav.cursor >= 0 && m_feedNav.cursor < m_feeds.size() ? &m_feeds[m_feedNav.cursor] : nullptr;
}

QVector<const RssRule*> RssView::visibleRules() const
{
	QVector<const RssRule*> out;
	const RssFeed* f = currentFeed();
	for (const RssRule& r : m_ruleList)
		if (f && r.feedId == f->id)
			out.push_back(&r);
	return out;
}

const RssRule* RssView::currentRule() const
{
	const auto v = visibleRules();
	return m_ruleNav.cursor >= 0 && m_ruleNav.cursor < v.size() ? v[m_ruleNav.cursor] : nullptr;
}

QString RssView::statusText() const
{
	return m_app.tr(" %1 feeds · %2 rules").arg(m_feeds.size()).arg(m_ruleList.size());
}

void RssView::draw(Canvas& c, HitMap& hits, const Rect& area)
{
	const Glyphs& g = Glyphs::get();
	const int lw = qBound(24, area.w * 2 / 5, 44);
	m_feedBox = { area.x, area.y, lw, area.h };
	m_ruleBox = { area.x + lw, area.y, area.w - lw, area.h };

	c.box(m_feedBox, m_rules ? Style::Border : Style::BorderFocus, m_app.tr("Feeds"), m_rules ? Style::Dim : Style::Hl);
	c.box(m_ruleBox, m_rules ? Style::BorderFocus : Style::Border, m_app.tr("Download rules"), m_rules ? Style::Hl : Style::Dim);
	hits.add(m_feedBox, Action::FocusPane, TagPane, 0);
	hits.add(m_ruleBox, Action::FocusPane, TagPane, 1);

	m_rows = area.h - 2 - 3;
	m_feedNav.clamp(m_feeds.size(), m_rows);
	const auto rules = visibleRules();
	m_ruleNav.clamp(rules.size(), m_rows);

	// feeds
	const Rect fin = m_feedBox.inset(1, 1);
	for (int i = 0; i < m_rows && m_feedNav.scroll + i < m_feeds.size(); i++)
	{
		const int idx = m_feedNav.scroll + i;
		const bool cur = idx == m_feedNav.cursor;
		c.put(fin.x, fin.y + i, fit((cur ? g.cursor : " ") + " " + m_feeds[idx].name, fin.w), cur ? (m_rules ? Style::SelectedDim : Style::Selected) : Style::Normal);
		hits.add({ fin.x, fin.y + i, fin.w, 1 }, Action::SelectRow, TagFeedRow, idx);
	}
	if (m_feeds.isEmpty())
		c.put(fin.x + 1, fin.y + 1, fit(m_loaded ? m_app.tr("No feeds. Press N to add one.") : m_app.tr("Loading…"), fin.w - 2), Style::Dim);
	if (const RssFeed* f = currentFeed())
	{
		c.put(fin.x, fin.bottom() - 3, fit(" " + m_app.tr("URL"), fin.w), Style::Dim);
		c.put(fin.x, fin.bottom() - 2, fit(" " + f->url, fin.w), Style::Normal);
	}

	// rules
	const Rect rin = m_ruleBox.inset(1, 1);
	const int exprW = qMax(10, rin.w - 2 - 1 - 9 - 1 - 12 - 1 - 12);
	c.put(rin.x, rin.y, fit("  " + m_app.tr("Expression"), exprW + 2) + " " + fit(m_app.tr("Episodes"), 9) + " " + fit(m_app.tr("Queue"), 12) + " " + fit(m_app.tr("From"), 12), Style::Header);
	c.fill({ rin.x + exprW + 2 + 1 + 9 + 1 + 12 + 1 + 12, rin.y, qMax(0, rin.w - (exprW + 2 + 1 + 9 + 1 + 12 + 1 + 12)), 1 }, Style::Header);
	for (int i = 0; i < m_rows - 1 && m_ruleNav.scroll + i < rules.size(); i++)
	{
		const int idx = m_ruleNav.scroll + i;
		const RssRule& r = *rules[idx];
		QString qname = r.queue;
		for (const QueueInfo& q : m_app.store().queues())
			if (q.uuid == r.queue)
				qname = q.name;
		const bool cur = idx == m_ruleNav.cursor;
		const QString line = (cur ? g.cursor : " ") + " " + fit(r.expression.isEmpty() ? m_app.tr("(every item)") : r.expression, exprW) + " "
			+ fit(r.tvs, 9) + " " + fit(qname, 12) + " " + fit(r.tvs == "none" ? QString() : r.from + (r.addPaused ? " paused" : ""), 12);
		c.put(rin.x, rin.y + 1 + i, fit(line, rin.w), cur ? (m_rules ? Style::Selected : Style::SelectedDim) : Style::Normal);
		hits.add({ rin.x, rin.y + 1 + i, rin.w, 1 }, Action::SelectRow, TagRuleRow, idx);
	}
	if (rules.isEmpty())
		c.put(rin.x + 1, rin.y + 2, fit(m_feeds.isEmpty() ? m_app.tr("Add a feed first.") : m_app.tr("No rules for this feed. Switch here with Tab and press N."), rin.w - 2), Style::Dim);
	if (const RssRule* r = currentRule())
	{
		c.put(rin.x, rin.bottom() - 3, fit(" " + m_app.tr("Target ") + r->target, rin.w), Style::Dim);
		QString flags;
		flags += m_app.tr("Paused: %1   Repacks: %2   Trailers: %3   Manual excluded: %4")
			.arg(r->addPaused ? m_app.tr("yes") : m_app.tr("no"), r->includeRepacks ? m_app.tr("yes") : m_app.tr("no"),
				r->includeTrailers ? m_app.tr("yes") : m_app.tr("no"), r->excludeManuals ? m_app.tr("yes") : m_app.tr("no"));
		c.put(rin.x, rin.bottom() - 2, fit(" " + flags, rin.w), Style::Dim);
		if (r->tvs != "none")
			c.put(rin.x, rin.bottom() - 1, fit(" " + m_app.tr("Range %1 – %2").arg(r->from, r->to), rin.w), Style::Dim);
	}
}

// ------------------------------------------------------------------ actions

void RssView::perform(Action a)
{
	ListNav& nav = m_rules ? m_ruleNav : m_feedNav;
	const int page = qMax(1, m_rows - 1);
	switch (a)
	{
		case Action::Up: nav.move(-1); if (!m_rules) m_ruleNav = ListNav(); return;
		case Action::Down: nav.move(1); if (!m_rules) m_ruleNav = ListNav(); return;
		case Action::PageUp: nav.move(-page); return;
		case Action::PageDown: nav.move(page); return;
		case Action::Home: nav.cursor = 0; return;
		case Action::End: nav.cursor = 1 << 20; return;
		case Action::FocusNext:
		case Action::FocusPrev:
		case Action::FocusPane:
			m_rules = !m_rules;
			return;
		case Action::Left: m_rules = false; return;
		case Action::Right: m_rules = true; return;
		case Action::NewItem: return m_rules ? ruleForm(nullptr) : feedForm(nullptr);
		case Action::EditItem:
			if (m_rules) { if (const RssRule* r = currentRule()) ruleForm(r); }
			else if (const RssFeed* f = currentFeed()) feedForm(f);
			return;
		case Action::DeleteItem: return deleteCurrent();
		case Action::RefreshFeeds:
			m_app.api().call("POST", "/rss/refresh", QJsonValue(), m_app.reply(m_app.tr("Cannot refresh"), [this]() {
				m_app.toast(m_app.tr("Fetching all feeds…"));
				m_dirty = true;
			}));
			return;
		case Action::TestRegexp: return testForm();
		case Action::DownloadedList: return downloadedForm();
		default: return;
	}
}

void RssView::feedForm(const RssFeed* existing)
{
	auto d = std::make_unique<FormDialog>(existing ? m_app.tr("Edit feed") : m_app.tr("New feed"), 74);
	d->addText("name", m_app.tr("Name"), existing ? existing->name : QString());
	d->addText("url", m_app.tr("URL"), existing ? existing->url : QString());
	const QString id = existing ? existing->id : QString();
	d->setSubmit(existing ? m_app.tr("Save") : m_app.tr("Add"), [this, id](FormDialog& f) {
		const QString name = f.text("name").trimmed(), url = f.text("url").trimmed();
		if (name.isEmpty() || url.isEmpty())
		{
			f.setError(m_app.tr("Both the name and the URL are needed"));
			return false;
		}
		QJsonObject o{ { "name", name }, { "url", url } };
		auto cb = m_app.reply(m_app.tr("Cannot save the feed"), [this]() { m_dirty = true; });
		if (id.isEmpty())
			m_app.api().call("POST", "/rss/feeds", o, cb);
		else
			m_app.api().call("PATCH", "/rss/feeds/" + ApiClient::encode(id), o, cb);
		m_dirty = true;
		return true;
	});
	m_app.push(std::move(d));
}

void RssView::ruleForm(const RssRule* existing)
{
	if (m_feeds.isEmpty())
	{
		m_app.toast(m_app.tr("Add a feed first"), true);
		return;
	}
	if (m_app.store().queues().isEmpty())
	{
		m_app.toast(m_app.tr("There is no queue to download into"), true);
		return;
	}
	QStringList fnames, fids, qnames, qids;
	for (const RssFeed& f : m_feeds) { fnames << f.name; fids << f.id; }
	for (const QueueInfo& q : m_app.store().queues()) { qnames << q.name; qids << q.uuid; }

	auto d = std::make_unique<FormDialog>(existing ? m_app.tr("Edit download rule") : m_app.tr("New download rule"), 80);
	int fi = 0;
	const RssFeed* cf = currentFeed();
	for (int i = 0; i < fids.size(); i++)
		if ((existing && fids[i] == existing->feedId) || (!existing && cf && fids[i] == cf->id))
			fi = i;
	int qi = m_app.store().queueIndex();
	if (existing)
		for (int i = 0; i < qids.size(); i++)
			if (qids[i] == existing->queue)
				qi = i;
	d->addSelect("feed", m_app.tr("Feed"), fnames, fids, fi);
	d->addText("expr", m_app.tr("Expression"), existing ? existing->expression : QString());
	d->addSelect("queue", m_app.tr("Queue"), qnames, qids, qi);
	d->addText("target", m_app.tr("Save to"), existing ? existing->target : (m_app.store().currentQueue() ? m_app.store().currentQueue()->defaultDirectory : QString()));
	d->addCheck("paused", m_app.tr("Add paused"), existing ? existing->addPaused : false);
	d->addSelect("tvs", m_app.tr("Episodes"), { m_app.tr("Not tracked"), m_app.tr("Season (S01E02)"), m_app.tr("Episode (0123)"), m_app.tr("Date (2026-10-16)") }, TvsValues,
		existing ? TvsValues.indexOf(existing->tvs) : 0);
	d->addText("from", m_app.tr("From episode"), existing ? existing->from : QString());
	d->addText("to", m_app.tr("To episode"), existing ? existing->to : QString());
	d->addCheck("repacks", m_app.tr("Include repacks"), existing ? existing->includeRepacks : false);
	d->addCheck("trailers", m_app.tr("Include trailers"), existing ? existing->includeTrailers : false);
	d->addCheck("manuals", m_app.tr("Exclude manual"), existing ? existing->excludeManuals : false);
	d->addText("link", m_app.tr("Link regexp"), existing ? existing->linkRegexp : QString());
	d->addShortcut('b', true, "Ctrl-B", m_app.tr("Browse"), [this](FormDialog& f) {
		dialogs::browseDirectory(m_app, f.text("target"), f.value("queue"), [dp = &f](const QString& p) { dp->setText("target", p); });
	});

	const QString id = existing ? existing->id : QString();
	d->setSubmit(existing ? m_app.tr("Save") : m_app.tr("Add"), [this, id](FormDialog& f) {
		QJsonObject o{ { "feedId", f.value("feed") }, { "expression", f.text("expr") }, { "queue", f.value("queue") },
			{ "target", f.text("target").trimmed() }, { "addPaused", f.checked("paused") }, { "tvs", f.value("tvs") },
			{ "includeRepacks", f.checked("repacks") }, { "includeTrailers", f.checked("trailers") },
			{ "excludeManuals", f.checked("manuals") }, { "linkRegexp", f.text("link") } };
		if (f.value("tvs") != "none" && (!f.text("from").trimmed().isEmpty() || !f.text("to").trimmed().isEmpty()))
		{
			o["from"] = f.text("from").trimmed();
			o["to"] = f.text("to").trimmed();
		}
		if (o["target"].toString().isEmpty())
			o.remove("target");
		auto cb = m_app.reply(m_app.tr("Cannot save the rule"), [this]() { m_dirty = true; });
		if (id.isEmpty())
			m_app.api().call("POST", "/rss/regexps", o, cb);
		else
			m_app.api().call("PATCH", "/rss/regexps/" + ApiClient::encode(id), o, cb);
		m_dirty = true;
		return true;
	});
	m_app.push(std::move(d));
}

void RssView::testForm()
{
	const RssRule* r = currentRule();
	auto d = std::make_unique<FormDialog>(m_app.tr("Test a regular expression"), 80);
	d->addText("expr", m_app.tr("Expression"), r ? r->expression : QString());
	d->addText("text", m_app.tr("Item title"));
	d->addSelect("tvs", m_app.tr("Episodes"), { m_app.tr("Not tracked"), m_app.tr("Season"), m_app.tr("Episode"), m_app.tr("Date") }, TvsValues, r ? TvsValues.indexOf(r->tvs) : 0);
	d->focusField("text");
	d->setSubmit(m_app.tr("Test"), [this](FormDialog& f) {
		if (f.text("text").isEmpty())
		{
			f.setError(m_app.tr("Type a title to test"));
			return false;
		}
		QJsonObject o{ { "expression", f.text("expr") }, { "text", f.text("text") }, { "tvs", f.value("tvs") } };
		m_app.api().call("POST", "/rss/test", o, [this](const ApiResult& res) {
			if (!res.ok())
			{
				m_app.toast(m_app.tr("Test failed: %1").arg(res.describe()), true);
				return;
			}
			const QJsonObject j = res.json.toObject();
			QString msg = j["matches"].toBool() ? m_app.tr("The expression matches") : m_app.tr("The expression does not match");
			if (j.contains("episode"))
				msg += m_app.tr(", episode %1").arg(j["episode"].toString());
			m_app.toast(msg, !j["matches"].toBool());
		});
		return true;
	});
	m_app.push(std::move(d));
}

void RssView::downloadedForm()
{
	const RssRule* r = currentRule();
	if (!r)
		return;
	const QString id = r->id;
	m_app.api().get("/rss/regexps/" + ApiClient::encode(id) + "/downloaded", [this, id](const ApiResult& res) {
		if (!res.ok())
		{
			m_app.toast(res.status == 409 ? m_app.tr("This rule does not track episodes") : res.describe(), true);
			return;
		}
		QStringList eps;
		for (const QJsonValue& v : res.json.toArray())
			eps << v.toString();
		auto d = std::make_unique<FormDialog>(m_app.tr("Downloaded episodes"), 80);
		d->addLabel(m_app.tr("Separate entries with spaces. Remove one to download that episode again."));
		d->addText("eps", m_app.tr("Episodes"), eps.join(' '));
		d->setSubmit(m_app.tr("Save"), [this, id](FormDialog& f) {
			QJsonArray a;
			for (const QString& s : f.text("eps").split(' ', Qt::SkipEmptyParts))
				a.append(s);
			m_app.api().call("PUT", "/rss/regexps/" + ApiClient::encode(id) + "/downloaded", a, m_app.reply(m_app.tr("Cannot save the list")));
			return true;
		});
		m_app.push(std::move(d));
	});
}

void RssView::deleteCurrent()
{
	if (m_rules)
	{
		const RssRule* r = currentRule();
		if (!r)
			return;
		const QString id = r->id;
		QVector<Choice> ch = { { "Y", m_app.tr("Yes, delete it"), 0 }, { "N", m_app.tr("No"), -1 } };
		m_app.push(std::make_unique<ChoiceDialog>(m_app.tr("Delete rule"), QStringList{ m_app.tr("Delete the rule \"%1\"?").arg(r->expression.isEmpty() ? m_app.tr("(every item)") : r->expression) }, ch, [this, id](int res) {
			if (res >= 0)
			{
				m_dirty = true;
				m_app.api().call("DELETE", "/rss/regexps/" + ApiClient::encode(id), QJsonValue(), m_app.reply(m_app.tr("Cannot delete the rule")));
			}
		}));
		return;
	}
	const RssFeed* f = currentFeed();
	if (!f)
		return;
	const QString id = f->id;
	int uses = 0;
	for (const RssRule& r : m_ruleList)
		uses += r.feedId == id;
	QStringList msg{ m_app.tr("Delete the feed \"%1\"?").arg(f->name) };
	if (uses)
		msg << m_app.tr("Its %1 download rule(s) are deleted too.").arg(uses);
	QVector<Choice> ch = { { "Y", m_app.tr("Yes, delete it"), 0 }, { "N", m_app.tr("No"), -1 } };
	m_app.push(std::make_unique<ChoiceDialog>(m_app.tr("Delete feed"), msg, ch, [this, id](int res) {
		if (res >= 0)
		{
			m_dirty = true;
			m_app.api().call("DELETE", "/rss/feeds/" + ApiClient::encode(id), QJsonValue(), m_app.reply(m_app.tr("Cannot delete the feed")));
		}
	}));
}

void RssView::menu(int x, int y)
{
	auto act = [this](Action a) { return [this, a]() { perform(a); }; };
	QVector<MenuItem> items;
	items << MenuItem{ m_app.tr("New"), "N", true, false, act(Action::NewItem) }
		<< MenuItem{ m_app.tr("Edit"), "E", true, false, act(Action::EditItem) }
		<< MenuItem{ m_app.tr("Delete…"), "D", true, false, act(Action::DeleteItem) }
		<< MenuItem::sep();
	if (m_rules)
		items << MenuItem{ m_app.tr("Test a regexp…"), "T", true, false, act(Action::TestRegexp) }
			<< MenuItem{ m_app.tr("Downloaded episodes…"), "W", true, false, act(Action::DownloadedList) };
	items << MenuItem{ m_app.tr("Refresh feeds now"), "R", true, false, act(Action::RefreshFeeds) };
	m_app.push(std::make_unique<MenuDialog>(items, x, y));
}

void RssView::mouse(const Event& e, const Hit* hit)
{
	const bool overRules = m_ruleBox.contains(e.x, e.y);
	if (e.mouse == Event::WheelUp || e.mouse == Event::WheelDown)
	{
		const int d = e.mouse == Event::WheelUp ? -1 : 1;
		if (overRules)
			m_ruleNav.move(d);
		else
		{
			m_feedNav.move(d);
			m_ruleNav = ListNav();
		}
		return;
	}
	if (e.mouse != Event::Press || !hit)
		return;
	if (hit->tag == TagFeedRow)
	{
		if (m_feedNav.cursor != hit->index)
			m_ruleNav = ListNav();
		m_rules = false;
		m_feedNav.cursor = hit->index;
	}
	else if (hit->tag == TagRuleRow)
	{
		m_rules = true;
		m_ruleNav.cursor = hit->index;
	}
	else if (hit->tag == TagPane)
	{
		m_rules = hit->index == 1;
	}
	else
		return;

	if (e.button == 3)
		return menu(e.x, e.y);
	if (e.button == 1 && e.doubleClick && (hit->tag == TagFeedRow || hit->tag == TagRuleRow))
		perform(Action::EditItem);
}

}
