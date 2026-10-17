#include "SchedulerPanel.h"
#include "Dialogs.h"
#include "Format.h"
#include <QDateTime>
#include <QJsonArray>
#include <QTime>

namespace tui
{

namespace scheduler
{

static const char* const Weekdays[] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };

Entry Entry::fromJson(const QJsonObject& o)
{
	Entry a;

	a.id = o["id"].toString();
	a.name = o["name"].toString();
	a.queue = o["queue"].toString();
	a.action = o["action"].toString();
	a.repeated = o["repeated"].toBool();
	a.time = o["time"].toString();
	a.at = o["at"].toString();
	for (const QJsonValue& d : o["days"].toArray())
		a.days << d.toBool();
	while (a.days.size() < 7)
		a.days << false;
	if (o["speedLimit"].isObject())
	{
		a.hasLimit = true;
		a.down = qint64(o["speedLimit"].toObject()["down"].toDouble());
		a.up = qint64(o["speedLimit"].toObject()["up"].toDouble());
	}
	return a;
}

QString daysLabel(const QVector<bool>& days)
{
	if (days.size() != 7)
		return QString();

	bool all = true;
	for (bool d : days)
		all = all && d;
	if (all)
		return QStringLiteral("Every day");

	QStringList parts;
	int i = 0;

	while (i < 7)
	{
		if (!days[i])
		{
			i++;
			continue;
		}

		int end = i;
		while (end + 1 < 7 && days[end + 1])
			end++;

		// a run of three or more is shown as a range
		if (end - i >= 2)
			parts << QString("%1-%2").arg(Weekdays[i], Weekdays[end]);
		else
			for (int d = i; d <= end; d++)
				parts << Weekdays[d];
		i = end + 1;
	}
	return parts.join(", ");
}

static QString rate(qint64 bytes)
{
	return bytes == 0 ? QStringLiteral("unlimited") : QString("%1 kB/s").arg(dialogs::kbText(bytes));
}

QString actionLabel(const Entry& a)
{
	if (a.action == "resume")
		return QStringLiteral("Resume all");
	if (a.action == "stop")
		return QStringLiteral("Stop all");
	if (a.hasLimit)
		return QString("Set speed limit (down %1, up %2)").arg(rate(a.down), rate(a.up));
	return QStringLiteral("Set speed limit");
}

QString whenLabel(const Entry& a)
{
	if (!a.repeated)
		return QString(a.at).replace('T', ' ');
	return (daysLabel(a.days) + " " + a.time).trimmed();
}

}

using scheduler::Entry;

SchedulerPanel::SchedulerPanel(App& app) : m_app(app)
{
}

SchedulerPanel::~SchedulerPanel()
{
	*m_alive = false;
}

void SchedulerPanel::load()
{
	std::weak_ptr<bool> alive = m_alive;

	m_state = State::Loading;
	m_app.api().schedulerActions([this, alive](const ApiResult& r) {
		if (alive.expired())
			return;
		if (!r.ok())
		{
			m_error = r.describe();
			m_state = State::Failed;
			return;
		}

		m_actions.clear();
		for (const QJsonValue& v : r.json.toArray())
			m_actions << Entry::fromJson(v.toObject());
		m_sel = qBound(0, m_sel, qMax(0, int(m_actions.size()) - 1));
		m_error.clear();
		m_state = State::Ready;
	});
}

void SchedulerPanel::select(int i)
{
	m_sel = qBound(0, i, qMax(0, int(m_actions.size()) - 1));
}

QVector<Hint> SchedulerPanel::hints()
{
	QVector<Hint> h;
	const QString enter = QString::fromUtf8("⏎");

	if (m_state == State::Failed)
	{
		h.push_back({ "R", m_app.tr("Retry"), [this]() { load(); } });
		return h;
	}
	h.push_back({ "A", m_app.tr("Add"), [this]() { edit(nullptr); } });
	if (!m_actions.isEmpty())
	{
		h.push_back({ enter, m_app.tr("Edit"), [this]() { edit(&m_actions[m_sel]); } });
		h.push_back({ "D", m_app.tr("Delete"), [this]() { remove(); } });
	}
	return h;
}

bool SchedulerPanel::key(const Event& e)
{
	if (m_state == State::Failed)
	{
		if (e.isLetter('r') || e.is(KeyEnter))
			load();
		return e.isLetter('r') || e.is(KeyEnter);
	}
	if (m_state != State::Ready)
		return false;

	if (e.is(KeyUp))
		select(m_sel - 1);
	else if (e.is(KeyDown))
		select(m_sel + 1);
	else if (e.is(KeyHome))
		select(0);
	else if (e.is(KeyEnd))
		select(m_actions.size() - 1);
	else if (e.isLetter('a'))
		edit(nullptr);
	else if (e.is(KeyEnter) || e.isLetter('e'))
	{
		if (m_actions.isEmpty())
			return false;
		edit(&m_actions[m_sel]);
	}
	else if (e.is(KeyDelete) || e.isLetter('d'))
		remove();
	else
		return false;
	return true;
}

void SchedulerPanel::remove()
{
	if (m_state != State::Ready || m_actions.isEmpty())
		return;

	const Entry a = m_actions[m_sel];
	std::weak_ptr<bool> alive = m_alive;
	App* app = &m_app;

	m_app.push(std::make_unique<ChoiceDialog>(m_app.tr("Delete action"), QStringList{ m_app.tr("Delete the scheduled action \"%1\"?").arg(a.name) },
		QVector<Choice>{ { "D", m_app.tr("Delete"), 1 }, { "C", m_app.tr("Cancel"), -1 } },
		[this, alive, app, a](int r) {
			if (r != 1)
				return;
			app->api().deleteSchedulerAction(a.id, app->reply(app->tr("Cannot delete the action"), [this, alive]() {
				if (!alive.expired())
					load();
			}));
		}));
}

// ------------------------------------------------------------------ the form

static const char* const ActionValues[] = { "resume", "stop", "speedLimit" };

void SchedulerPanel::edit(const Entry* existing)
{
	const QVector<QueueInfo>& queues = m_app.store().queues();

	if (queues.isEmpty())
		return m_app.toast(m_app.tr("Add a queue first"), true);

	QStringList queueNames, queueIds;
	int queueIndex = 0;
	const QString current = existing ? existing->queue : (m_app.store().currentQueue() ? m_app.store().currentQueue()->uuid : QString());

	for (int i = 0; i < queues.size(); i++)
	{
		queueNames << queues[i].name;
		queueIds << queues[i].uuid;
		if (queues[i].uuid == current)
			queueIndex = i;
	}

	Entry a;
	if (existing)
		a = *existing;
	else
	{
		a.days = QVector<bool>(7, true);
		a.time = QTime::currentTime().toString("HH:mm");
		a.at = QDateTime::currentDateTime().toString("yyyy-MM-ddTHH:mm");
	}

	int actionIndex = 0;
	for (int i = 0; i < 3; i++)
		if (a.action == ActionValues[i])
			actionIndex = i;

	auto d = std::make_unique<FormDialog>(existing ? m_app.tr("Edit action") : m_app.tr("New action"), 70);
	d->addText("name", m_app.tr("Name"), a.name);
	d->addSelect("queue", m_app.tr("Queue"), queueNames, queueIds, queueIndex);
	d->addSelect("action", m_app.tr("Action"), { m_app.tr("Resume all"), m_app.tr("Stop all"), m_app.tr("Set speed limit") },
		{ ActionValues[0], ActionValues[1], ActionValues[2] }, actionIndex);
	d->addText("sdown", m_app.tr("Speed limit down"), a.hasLimit ? dialogs::kbText(a.down) : "0", "KB/s, 0 = none");
	d->addText("sup", m_app.tr("Speed limit up"), a.hasLimit ? dialogs::kbText(a.up) : "0", "KB/s, 0 = none");
	d->addLabel(m_app.tr("The speed limits apply to the \"Set speed limit\" action only."));
	d->addSpacer();
	d->addSelect("repeated", m_app.tr("When"), { m_app.tr("Repeated"), m_app.tr("One time") }, { "repeated", "once" }, a.repeated ? 0 : 1);
	d->addText("time", m_app.tr("Time"), a.time, "HH:mm, repeated");
	for (int i = 0; i < 7; i++)
		d->addCheck(QString("day%1").arg(i), scheduler::Weekdays[i], a.days[i]);
	d->addText("at", m_app.tr("Date and time"), QString(a.at).replace('T', ' '), "yyyy-MM-dd HH:mm, one time");

	const QString id = existing ? existing->id : QString();
	std::weak_ptr<bool> alive = m_alive;
	App* app = &m_app;

	d->setSubmit(existing ? m_app.tr("Save") : m_app.tr("Create"), [this, app, id, alive](FormDialog& f) {
		const QString name = f.text("name").trimmed();
		if (name.isEmpty())
		{
			f.focusField("name");
			f.setError(app->tr("The action needs a name"));
			return false;
		}

		QJsonObject o{ { "name", name }, { "queue", f.value("queue") }, { "action", f.value("action") } };

		if (f.value("action") == "speedLimit")
		{
			bool ok1, ok2;
			const qint64 down = dialogs::parseKb(f.text("sdown"), &ok1), up = dialogs::parseKb(f.text("sup"), &ok2);

			if (!ok1 || !ok2 || down > INT_MAX || up > INT_MAX)
			{
				f.focusField(ok1 ? "sup" : "sdown");
				f.setError(app->tr("Speed limits must be whole numbers of KB/s"));
				return false;
			}
			o["speedLimit"] = QJsonObject{ { "down", int(down) }, { "up", int(up) } };
		}

		const bool repeated = f.value("repeated") == "repeated";

		o["repeated"] = repeated;
		if (repeated)
		{
			const QString t = f.text("time").trimmed();
			if (t.size() != 5 || !QTime::fromString(t, "HH:mm").isValid())
			{
				f.focusField("time");
				f.setError(app->tr("The time must be of the form HH:mm"));
				return false;
			}

			QJsonArray days;
			bool any = false;
			for (int i = 0; i < 7; i++)
			{
				const bool on = f.checked(QString("day%1").arg(i));
				days << on;
				any = any || on;
			}
			if (!any)
			{
				f.focusField("day0");
				f.setError(app->tr("Select at least one day"));
				return false;
			}
			o["time"] = t;
			o["days"] = days;
		}
		else
		{
			const QString t = f.text("at").trimmed();
			if (t.size() != 16 || !QDateTime::fromString(t, "yyyy-MM-dd HH:mm").isValid())
			{
				f.focusField("at");
				f.setError(app->tr("The date must be of the form yyyy-MM-dd HH:mm"));
				return false;
			}
			o["at"] = QString(t).replace(' ', 'T');
		}

		auto done = app->reply(app->tr("Cannot save the action"), [this, alive]() {
			if (!alive.expired())
				load();
		});
		if (id.isEmpty())
			app->api().createSchedulerAction(o, done);
		else
			app->api().patchSchedulerAction(id, o, done);
		return true;
	});
	m_app.push(std::move(d));
}

// ------------------------------------------------------------------ drawing

void SchedulerPanel::draw(Canvas& c, const Rect& area, bool active, const AddClick& addClick)
{
	if (m_state == State::Idle || m_state == State::Loading)
		return c.put(area.x, area.y, m_app.tr("Loading..."), Style::Dim);
	if (m_state == State::Failed)
	{
		c.put(area.x, area.y, fit(m_app.tr("Cannot load the scheduled actions"), area.w), Style::Red);
		c.put(area.x, area.y + 1, fit(m_error, area.w), Style::Dim);
		return;
	}

	c.put(area.x, area.y, fit(m_app.tr("Scheduled actions are saved as soon as you change them."), area.w), Style::Dim);

	const bool queueCol = area.w >= 64;
	const int nameW = qMax(8, area.w * 22 / 100), queueW = queueCol ? area.w * 16 / 100 : 0, whenW = qMax(12, area.w * 26 / 100);
	const int actionW = qMax(0, area.w - nameW - queueW - whenW - 3);
	const int listY = area.y + 2, rows = qMax(0, area.h - 4);

	auto line = [&](const QString& n, const QString& q, const QString& act, const QString& when) {
		QString s = fit(n, nameW) + " ";
		if (queueCol)
			s += fit(q, queueW) + " ";
		return s + fit(when, whenW) + " " + fit(act, actionW);
	};

	c.put(area.x, area.y + 1, fit(line(m_app.tr("Name"), m_app.tr("Queue"), m_app.tr("Action"), m_app.tr("When")), area.w), Style::Header);

	if (m_actions.isEmpty())
		c.put(area.x, listY, fit(m_app.tr("No scheduled actions yet. Press A to add one."), area.w), Style::Dim);

	int top = 0;
	if (m_actions.size() > rows)
		top = qBound(0, m_sel - rows / 2, m_actions.size() - rows);

	for (int i = top; i < m_actions.size() && i - top < rows; i++)
	{
		const Entry& a = m_actions[i];
		QString queue = a.queue;
		for (const QueueInfo& q : m_app.store().queues())
			if (q.uuid == a.queue)
				queue = q.name;

		const bool sel = i == m_sel;
		const Rect r = { area.x, listY + i - top, area.w, 1 };

		c.put(r.x, r.y, fit(line(a.name, queue, scheduler::actionLabel(a), scheduler::whenLabel(a)), r.w),
			sel ? (active ? Style::Hl : Style::Selected) : Style::Normal);
		addClick(r, [this, i]() {
			// a second click on the selected row opens it
			if (i == m_sel)
				edit(&m_actions[i]);
			else
				select(i);
		});
	}

	const QString add = QStringLiteral("[ ") + m_app.tr("Add action...") + " ]";
	const Rect r = { area.x, area.bottom() - 1, int(add.size()), 1 };
	c.put(r.x, r.y, add, Style::Normal);
	addClick(r, [this]() { edit(nullptr); });
}

}
