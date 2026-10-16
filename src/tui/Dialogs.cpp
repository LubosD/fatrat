#include "Dialogs.h"
#include <QRegularExpression>
#include "Format.h"
#include <QDir>
#include <QFileInfo>

namespace tui
{
namespace dialogs
{

qint64 parseKb(const QString& s, bool* ok)
{
	const QString t = s.trimmed();
	if (t.isEmpty())
	{
		*ok = true;
		return 0;
	}
	bool good;
	const qint64 v = t.toLongLong(&good);
	*ok = good && v >= 0 && v < (qint64(1) << 40); // far beyond any real link, and keeps the product in range
	return v * 1024;
}

QString kbText(qint64 bytesPerSecond)
{
	return QString::number(bytesPerSecond / 1024);
}

QString expandPath(const QString& p)
{
	if (p == "~" || p.startsWith("~/"))
		return QDir::homePath() + p.mid(1);
	if (p.startsWith("file://"))
		return p.mid(7);
	return p;
}

static QJsonObject limitsJson(qint64 down, qint64 up)
{
	return QJsonObject{ { "down", double(down) }, { "up", double(up) } };
}

// ------------------------------------------------------------ directories

class DirBrowser : public Dialog
{
public:
	DirBrowser(App& app, const QString& start, const QString& queue, std::function<void(const QString&)> chosen)
		: m_app(app), m_queue(queue), m_chosen(std::move(chosen))
	{
		load(start);
	}

	void draw(Canvas& c, HitMap& hits, const Rect& screen) override
	{
		beginDraw(hits, screen);
		m_rect = place(screen, qMin(72, screen.w), qMin(22, screen.h));
		drawFrame(c, m_app.tr("Choose a directory on the server"));
		const int x = m_rect.x + 2, w = m_rect.w - 4;
		c.put(x, m_rect.y + 1, fit(m_loading ? m_app.tr("Loading…") : m_path, w), Style::Hl);
		if (!m_error.isEmpty())
			c.put(x, m_rect.y + 2, fit(m_error, w), Style::Red);

		m_rows = m_rect.h - 6;
		QStringList names;
		if (!m_parent.isEmpty())
			names << "..";
		for (const QString& n : m_names)
			names << n + "/";
		m_cur = qBound(0, m_cur, qMax(0, int(names.size()) - 1));
		if (m_cur < m_top)
			m_top = m_cur;
		if (m_cur >= m_top + m_rows)
			m_top = m_cur - m_rows + 1;

		for (int i = 0; i < m_rows && m_top + i < names.size(); i++)
		{
			const int idx = m_top + i, y = m_rect.y + 3 + i;
			const bool sel = idx == m_cur;
			c.put(x, y, fit(names[idx], w), sel ? Style::Selected : Style::Normal);
			addClick({ x, y, w, 1 }, [this, idx]() { m_cur = idx; });
		}
		if (names.isEmpty() && !m_loading)
			c.put(x, m_rect.y + 3, m_app.tr("(no subdirectories)"), Style::Dim);

		drawHints(c, m_rect.x + 1, m_rect.bottom() - 2, m_rect.w - 2, {
			{ QString::fromUtf8("⏎"), m_app.tr("Open"), [this]() { open(); } },
			{ QString::fromUtf8("⌫"), m_app.tr("Up"), [this]() { goUp(); } },
			{ "C", m_app.tr("Choose"), [this]() { choose(); } },
			{ "H", m_app.tr("Hidden"), [this]() { m_hidden = !m_hidden; load(m_path); } },
			{ "Esc", m_app.tr("Cancel"), [this]() { close(); } } });
	}

	void handle(const Event& e) override
	{
		if (e.type == Event::Mouse)
		{
			if (e.mouse == Event::WheelUp) m_cur = qMax(0, m_cur - 3);
			else if (e.mouse == Event::WheelDown) m_cur += 3;
			else
			{
				const int before = m_cur;
				handleMouse(e);
				if (e.mouse == Event::Press && e.doubleClick && before == m_cur)
					open();
			}
			return;
		}
		if (e.type != Event::Key)
			return;
		if (e.is(KeyEscape)) return close();
		if (e.key == KeyUp || e.isLetter('k')) m_cur--;
		else if (e.key == KeyDown || e.isLetter('j')) m_cur++;
		else if (e.key == KeyPageUp) m_cur -= m_rows;
		else if (e.key == KeyPageDown) m_cur += m_rows;
		else if (e.key == KeyHome) m_cur = 0;
		else if (e.key == KeyEnd) m_cur = 1 << 20;
		else if (e.is(KeyEnter) || e.key == KeyRight) open();
		else if (e.key == KeyBackspace || e.key == KeyLeft) goUp();
		else if (e.isLetter('c')) choose();
		else if (e.isLetter('h')) { m_hidden = !m_hidden; load(m_path); }
	}

private:
	void load(const QString& path)
	{
		m_loading = true;
		const int gen = ++m_gen;
		auto alive = m_alive;
		m_app.api().listDirectories(path, m_queue, m_hidden, [this, gen, alive](const ApiResult& r) {
			if (!*alive || gen != m_gen)
				return;
			m_loading = false;
			if (!r.ok())
			{
				m_error = r.describe();
				return;
			}
			m_error.clear();
			const QJsonObject o = r.json.toObject();
			m_path = o["path"].toString();
			m_parent = o["parent"].toString();
			m_names.clear();
			m_paths.clear();
			for (const QJsonValue& v : o["entries"].toArray())
			{
				m_names << v.toObject()["name"].toString();
				m_paths << v.toObject()["path"].toString();
			}
			m_cur = 0;
			m_top = 0;
		});
	}

	void open()
	{
		const bool hasParent = !m_parent.isEmpty();
		if (hasParent && m_cur == 0)
			return goUp();
		const int i = m_cur - (hasParent ? 1 : 0);
		if (i >= 0 && i < m_paths.size())
			load(m_paths[i]);
	}

	void goUp()
	{
		if (!m_parent.isEmpty())
			load(m_parent);
	}

	void choose()
	{
		if (m_path.isEmpty())
			return;
		auto fn = m_chosen;
		const QString p = m_path;
		close();
		if (fn)
			fn(p);
	}

	App& m_app;
	QString m_queue, m_path, m_parent, m_error;
	QStringList m_names, m_paths;
	std::function<void(const QString&)> m_chosen;
	int m_cur = 0, m_top = 0, m_rows = 10, m_gen = 0;
	bool m_loading = true, m_hidden = false;
	std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);

public:
	~DirBrowser() override { *m_alive = false; }
};

void browseDirectory(App& app, const QString& start, const QString& queue, std::function<void(const QString&)> chosen)
{
	app.push(std::make_unique<DirBrowser>(app, start, queue, std::move(chosen)));
}

// ---------------------------------------------------------------- prompts

void prompt(App& app, const QString& title, const QString& label, const QString& initial, const QString& okLabel, std::function<void(const QString&)> done)
{
	auto d = std::make_unique<FormDialog>(title, 60);
	d->addText("v", label, initial);
	d->setSubmit(okLabel, [done](FormDialog& f) {
		done(f.text("v"));
		return true;
	});
	app.push(std::move(d));
}

// -------------------------------------------------------------- transfers

void newTransfer(App& app)
{
	app.api().transferClasses([&app](const ApiResult& r) {
		QStringList engines = { app.tr("Autodetect") }, values = { QString() }, modes = { "Download" };
		for (const QJsonValue& v : r.json.toArray())
		{
			const QJsonObject o = v.toObject();
			engines << QString("%1 (%2)").arg(o["longName"].toString(), o["mode"].toString().toLower());
			values << o["shortName"].toString();
			modes << o["mode"].toString();
		}

		const Store& st = app.store();
		QStringList qnames, quuids;
		for (const QueueInfo& q : st.queues())
		{
			qnames << q.name;
			quuids << q.uuid;
		}
		if (quuids.isEmpty())
		{
			app.toast(app.tr("There is no queue to add transfers to"), true);
			return;
		}

		auto d = std::make_unique<FormDialog>(app.tr("New transfer"), 78);
		FormField& links = d->addText("links", app.tr("Links"));
		links.placeholder = app.tr("URLs, magnet links, or the path of a .torrent file");
		d->addSpacer();
		d->addSelect("engine", app.tr("Engine"), engines, values, 0);
		d->addSelect("queue", app.tr("Queue"), qnames, quuids, st.queueIndex());
		const QueueInfo* cq = st.currentQueue();
		d->addText("target", app.tr("Save to"), cq ? cq->defaultDirectory : QString());
		d->addText("down", app.tr("Speed limit down"), "0", "KB/s, 0 = none");
		d->addText("up", app.tr("Speed limit up"), "0", "KB/s, 0 = none");
		d->addCheck("paused", app.tr("Start paused"), false);

		// follow the default directory of the chosen queue, unless the user changed it
		auto lastDefault = std::make_shared<QString>(cq ? cq->defaultDirectory : QString());
		d->field("queue")->onChange = [&app, dp = d.get(), lastDefault]() {
			const QString uuid = dp->value("queue");
			for (const QueueInfo& q : app.store().queues())
				if (q.uuid == uuid)
				{
					if (dp->text("target") == *lastDefault)
						dp->setText("target", q.defaultDirectory);
					*lastDefault = q.defaultDirectory;
				}
		};

		d->addShortcut('b', true, "Ctrl-B", app.tr("Browse"), [&app](FormDialog& f) {
			browseDirectory(app, f.text("target"), f.value("queue"), [dp = &f](const QString& p) { dp->setText("target", p); });
		});

		d->setSubmit(app.tr("Add"), [&app, modes](FormDialog& f) {
			// a path with spaces in it is one file, not several links
			const QString whole = f.text("links").trimmed();
			QStringList parts = f.text("links").split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
			if (parts.size() > 1 && !whole.contains("://") && QFileInfo(expandPath(whole)).isFile())
				parts = QStringList{ whole };
			if (parts.isEmpty())
			{
				f.setError(app.tr("Enter at least one link"));
				return false;
			}
			bool ok1, ok2;
			AddRequest req;
			req.downLimit = parseKb(f.text("down"), &ok1);
			req.upLimit = parseKb(f.text("up"), &ok2);
			if (!ok1 || !ok2)
			{
				f.setError(app.tr("Speed limits must be whole numbers"));
				return false;
			}
			req.cls = f.value("engine");
			req.upload = modes.value(f.index("engine")) == "Upload";
			req.target = f.text("target").trimmed();
			req.paused = f.checked("paused");
			const QString queue = f.value("queue");

			const QString local = expandPath(parts.first());
			if (parts.size() == 1 && !parts.first().contains("://") && QFileInfo(local).isFile())
			{
				app.api().uploadTransfer(queue, local, req, app.reply(app.tr("Cannot add the file"), [&app]() { app.toast(app.tr("Added the file")); }));
			}
			else
			{
				req.urls = parts;
				const int n = parts.size();
				app.api().addTransfers(queue, req, app.reply(app.tr("Cannot add the transfer"), [&app, n]() {
					app.toast(n == 1 ? app.tr("Added 1 transfer") : app.tr("Added %1 transfers").arg(n));
				}));
			}
			return true;
		});
		app.push(std::move(d));
	});
}

void editTransfer(App& app, const TransferInfo& t)
{
	auto d = std::make_unique<FormDialog>(app.tr("Transfer properties"), 74);
	d->addLabel(t.name);
	d->addSpacer();
	d->addText("comment", app.tr("Comment"), t.comment);
	d->addText("object", app.tr("Source"), t.object);
	d->addText("down", app.tr("Speed limit down"), kbText(t.userSpeedLimits.down), "KB/s, 0 = none");
	d->addText("up", app.tr("Speed limit up"), kbText(t.userSpeedLimits.up), "KB/s, 0 = none");
	const QString uuid = t.uuid;
	const TransferInfo orig = t;
	d->setSubmit(app.tr("Save"), [&app, uuid, orig](FormDialog& f) {
		bool ok1, ok2;
		const qint64 down = parseKb(f.text("down"), &ok1), up = parseKb(f.text("up"), &ok2);
		if (!ok1 || !ok2)
		{
			f.setError(app.tr("Speed limits must be whole numbers"));
			return false;
		}
		// send only what changed: the limits are shown in whole KB/s and must not be rounded silently
		QJsonObject o;
		if (f.text("comment") != orig.comment)
			o["comment"] = f.text("comment");
		if (f.text("object") != orig.object)
			o["object"] = f.text("object");
		if (f.text("down") != kbText(orig.userSpeedLimits.down) || f.text("up") != kbText(orig.userSpeedLimits.up))
			o["userSpeedLimits"] = limitsJson(down, up);
		if (o.isEmpty())
			return true;
		app.api().patchTransfer(uuid, o, app.reply(app.tr("Cannot save the properties")));
		return true;
	});
	app.push(std::move(d));
}

void limitTransfers(App& app, const QStringList& uuids, const SpeedPair& initial)
{
	auto d = std::make_unique<FormDialog>(app.tr("Speed limit"), 62);
	d->addLabel(uuids.size() == 1 ? app.tr("Limit for the selected transfer") : app.tr("Limit for %1 transfers").arg(uuids.size()));
	d->addSpacer();
	d->addText("down", app.tr("Download"), kbText(initial.down), "KB/s, 0 = none");
	d->addText("up", app.tr("Upload"), kbText(initial.up), "KB/s, 0 = none");
	d->setSubmit(app.tr("Apply"), [&app, uuids](FormDialog& f) {
		bool ok1, ok2;
		const qint64 down = parseKb(f.text("down"), &ok1), up = parseKb(f.text("up"), &ok2);
		if (!ok1 || !ok2)
		{
			f.setError(app.tr("Speed limits must be whole numbers"));
			return false;
		}
		app.api().patchTransfers(uuids, QJsonObject{ { "userSpeedLimits", limitsJson(down, up) } }, app.reply(app.tr("Cannot set the limit")));
		return true;
	});
	app.push(std::move(d));
}

void deleteTransfers(App& app, const QStringList& uuids, const QStringList& names)
{
	QStringList msg;
	msg << (uuids.size() == 1 ? app.tr("Delete this transfer?") : app.tr("Delete %1 transfers?").arg(uuids.size())) << "";
	for (int i = 0; i < names.size() && i < 5; i++)
		msg << "  " + elide(names[i], 56);
	if (names.size() > 5)
		msg << app.tr("  … and %1 more").arg(names.size() - 5);
	QVector<Choice> ch = { { "Y", app.tr("Yes, keep the files"), 0 }, { "D", app.tr("Delete with data"), 1 }, { "N", app.tr("No"), -1 } };
	app.push(std::make_unique<ChoiceDialog>(app.tr("Delete"), msg, ch, [&app, uuids](int r) {
		if (r < 0)
			return;
		app.store().clearMarks();
		app.api().deleteTransfers(uuids, r == 1, app.reply(app.tr("Cannot delete")));
	}));
}

void sendToQueue(App& app, const QStringList& uuids, int x, int y)
{
	QVector<MenuItem> items;
	const QString cur = app.store().currentQueueUuid();
	for (const QueueInfo& q : app.store().queues())
	{
		if (q.uuid == cur)
			continue;
		MenuItem it;
		it.label = q.name;
		const QString dest = q.uuid;
		it.fn = [&app, uuids, dest]() {
			app.store().clearMarks();
			app.api().patchTransfers(uuids, QJsonObject{ { "queue", dest } }, app.reply(app.tr("Cannot send to the queue")));
		};
		items << it;
	}
	if (items.isEmpty())
	{
		app.toast(app.tr("There is no other queue"), true);
		return;
	}
	app.push(std::make_unique<MenuDialog>(items, x, y, app.tr("Send to queue")));
}

// ----------------------------------------------------------------- queues

void queueEditor(App& app, const QueueInfo* existing)
{
	auto d = std::make_unique<FormDialog>(existing ? app.tr("Edit queue") : app.tr("New queue"), 78);
	d->addText("name", app.tr("Name"), existing ? existing->name : QString());
	d->addText("dir", app.tr("Default directory"), existing ? existing->defaultDirectory : QString());
	d->addText("move", app.tr("Move finished to"), existing ? existing->moveDirectory : QString());
	d->addCheck("upAsDown", app.tr("Upload as download"), existing ? existing->upAsDown : false);
	d->addSpacer();
	d->addText("sdown", app.tr("Speed limit down"), existing ? kbText(existing->speedLimits.down) : "0", "KB/s, 0 = none");
	d->addText("sup", app.tr("Speed limit up"), existing ? kbText(existing->speedLimits.up) : "0", "KB/s, 0 = none");
	d->addText("tdown", app.tr("Active downloads"), existing ? QString::number(existing->transferLimits.down) : "0", "at most, 0 = no limit");
	d->addText("tup", app.tr("Active uploads"), existing ? QString::number(existing->transferLimits.up) : "0", "at most, 0 = no limit");

	d->addShortcut('b', true, "Ctrl-B", app.tr("Browse"), [&app, uuid = existing ? existing->uuid : QString()](FormDialog& f) {
		const QString id = f.focusedId();
		if (id != "dir" && id != "move")
		{
			app.toast(app.tr("Select a directory field first"), true);
			return;
		}
		browseDirectory(app, f.text(id), uuid, [dp = &f, id](const QString& p) { dp->setText(id, p); });
	});

	const QString uuid = existing ? existing->uuid : QString();
	d->setSubmit(existing ? app.tr("Save") : app.tr("Create"), [&app, uuid](FormDialog& f) {
		const QString name = f.text("name").trimmed();
		if (name.isEmpty())
		{
			f.setError(app.tr("The queue needs a name"));
			return false;
		}
		bool ok1, ok2;
		const qint64 sd = parseKb(f.text("sdown"), &ok1), su = parseKb(f.text("sup"), &ok2);
		bool ok3, ok4;
		const qint64 td = f.text("tdown").trimmed().isEmpty() ? 0 : f.text("tdown").trimmed().toLongLong(&ok3);
		const qint64 tu = f.text("tup").trimmed().isEmpty() ? 0 : f.text("tup").trimmed().toLongLong(&ok4);
		ok3 = f.text("tdown").trimmed().isEmpty() || (td >= 0 && QString::number(td) == f.text("tdown").trimmed());
		ok4 = f.text("tup").trimmed().isEmpty() || (tu >= 0 && QString::number(tu) == f.text("tup").trimmed());
		if (!ok1 || !ok2 || !ok3 || !ok4)
		{
			f.setError(app.tr("Limits must be whole numbers"));
			return false;
		}
		QJsonObject o{ { "name", name }, { "defaultDirectory", f.text("dir").trimmed() }, { "moveDirectory", f.text("move").trimmed() },
			{ "upAsDown", f.checked("upAsDown") }, { "speedLimits", limitsJson(sd, su) }, { "transferLimits", limitsJson(td, tu) } };
		auto done = app.reply(app.tr("Cannot save the queue"), [&app]() { app.refreshQueues(); });
		if (uuid.isEmpty())
			app.api().createQueue(o, done);
		else
			app.api().patchQueue(uuid, o, done);
		return true;
	});
	app.push(std::move(d));
}

void deleteQueue(App& app, const QueueInfo& q)
{
	const int count = app.store().queueCount(q.uuid);
	const QString uuid = q.uuid;

	if (count == 0)
	{
		QVector<Choice> ch = { { "Y", app.tr("Yes, delete it"), 0 }, { "N", app.tr("No"), -1 } };
		app.push(std::make_unique<ChoiceDialog>(app.tr("Delete queue"), QStringList{ app.tr("Delete the empty queue \"%1\"?").arg(q.name) }, ch,
			[&app, uuid](int r) {
				if (r >= 0)
					app.api().deleteQueue(uuid, DeleteQueueOptions(), app.reply(app.tr("Cannot delete the queue"), [&app]() { app.refreshQueues(); }));
			}));
		return;
	}

	QStringList others, otherIds;
	for (const QueueInfo& o : app.store().queues())
		if (o.uuid != q.uuid)
		{
			others << o.name;
			otherIds << o.uuid;
		}

	auto d = std::make_unique<FormDialog>(app.tr("Delete queue"), 70);
	d->addLabel(count > 0 ? app.tr("Delete the queue \"%1\"? It holds %2 transfers.").arg(q.name).arg(count) : app.tr("Delete the queue \"%1\"?").arg(q.name));
	d->addSpacer();
	QStringList opts;
	QString keys;
	if (!others.isEmpty())
	{
		opts << app.tr("Move the transfers to another queue");
		keys += "m";
	}
	opts << app.tr("Delete the transfers, keep their files") << app.tr("Delete the transfers with their data");
	keys += "dw";
	d->addRadio("what", QString(), opts, keys, 0);
	if (!others.isEmpty())
		d->addSelect("dest", app.tr("Move to"), others, otherIds, 0);
	const bool canMove = !others.isEmpty();
	d->setSubmit(app.tr("Delete"), [&app, uuid, canMove](FormDialog& f) {
		int choice = f.index("what");
		if (!canMove)
			choice++;
		DeleteQueueOptions o;
		if (choice == 0)
			o.moveTo = f.value("dest");
		else
		{
			o.deleteTransfers = true;
			o.withData = choice == 2;
		}
		app.api().deleteQueue(uuid, o, app.reply(app.tr("Cannot delete the queue"), [&app]() { app.refreshQueues(); }));
		return true;
	});
	app.push(std::move(d));
}

}
}
