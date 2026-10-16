#include "SettingsDialog.h"
#include "Dialogs.h"
#include "Format.h"
#include <QJsonArray>

namespace tui
{

SettingsDialog::SettingsDialog(App& app) : m_app(app)
{
	loadPages();
}

SettingsDialog::~SettingsDialog()
{
	*m_alive = false;
}

// ------------------------------------------------------------------ loading

void SettingsDialog::loadPages()
{
	std::weak_ptr<bool> alive = m_alive;
	m_app.api().settingsPages([this, alive](const ApiResult& r) {
		if (alive.expired())
			return;
		if (!r.ok())
			return fail(r.describe());

		QStringList keys;
		for (const QJsonValue& v : r.json.toArray())
		{
			auto p = std::make_unique<Page>();
			p->title = v.toObject()["title"].toString();
			p->script = v.toObject()["script"].toString();
			p->spec = settingsPageSpec(p->script);
			if (p->spec)
				keys << p->spec->keys();
			m_pages.push_back(std::move(p));
		}
		if (m_pages.empty())
			return fail(m_app.tr("The server reports no settings pages."));
		keys.removeDuplicates();
		if (keys.isEmpty())
			buildPages(QJsonObject());
		else
			loadValues(keys);
	});
}

void SettingsDialog::loadValues(const QStringList& keys)
{
	std::weak_ptr<bool> alive = m_alive;
	m_app.api().settingsValues(keys, [this, alive](const ApiResult& r) {
		if (alive.expired())
			return;
		if (!r.ok())
			return fail(r.describe());
		buildPages(r.json.toObject());
	});
}

void SettingsDialog::buildPages(const QJsonObject& values)
{
	for (auto& p : m_pages)
	{
		p->loaded = values;
		if (p->spec)
			buildSettingsPanel(*p->spec, values, p->panel);
		else
			p->panel.addLabel(m_app.tr("This settings page isn't supported in this UI yet."));
	}
	m_sel = 0;
	m_state = State::Ready;
}

void SettingsDialog::fail(const QString& msg)
{
	m_error = msg;
	m_state = State::Failed;
}

// ------------------------------------------------------------------ state

void SettingsDialog::select(int index)
{
	if (index >= 0 && index < int(m_pages.size()))
		m_sel = index;
}

void SettingsDialog::selectRelative(int dir)
{
	select(qBound(0, m_sel + dir, int(m_pages.size()) - 1));
}

bool SettingsDialog::dirty() const
{
	for (const auto& p : m_pages)
	{
		if (!p->spec)
			continue;
		QJsonObject changes;
		if (!collectSettings(*p->spec, p->panel, p->loaded, changes, nullptr, nullptr) || !changes.isEmpty())
			return true;
	}
	return false;
}

void SettingsDialog::requestCancel()
{
	if (!dirty())
		return close();
	std::weak_ptr<bool> alive = m_alive;
	m_app.push(std::make_unique<ChoiceDialog>(m_app.tr("Settings"), QStringList{ m_app.tr("Discard the changes?") },
		QVector<Choice>{ { "D", m_app.tr("Discard"), 1 }, { "K", m_app.tr("Keep editing"), -1 } },
		[this, alive](int r) {
			if (r == 1 && !alive.expired())
				close();
		}));
}

// ------------------------------------------------------------------ saving

// Gathers the changes of every page; on a bad value shows the page and the field
bool SettingsDialog::collect(QJsonObject& patch)
{
	for (int i = 0; i < int(m_pages.size()); i++)
	{
		Page& p = *m_pages[i];
		if (!p.spec)
			continue;
		QString error, key;
		QJsonObject changes;
		if (!collectSettings(*p.spec, p.panel, p.loaded, changes, &error, &key))
		{
			select(i);
			m_formFocus = true;
			p.panel.focusField(key);
			m_error = error;
			return false;
		}
		for (auto it = changes.begin(); it != changes.end(); ++it)
			patch[it.key()] = it.value();
	}
	return true;
}

void SettingsDialog::save()
{
	if (m_saving || m_state != State::Ready)
		return;
	m_error.clear();
	QJsonObject patch;
	if (!collect(patch))
		return;
	if (patch.isEmpty())
		return close();

	// Changing the web interface while connected through it cuts the connection
	const bool remote = patch.contains("remote/enable") || patch.contains("remote/port") || patch.contains("remote/ssl");
	if (remote && m_app.api().http()->endpoint().kind != Endpoint::Unix)
	{
		std::weak_ptr<bool> alive = m_alive;
		m_app.push(std::make_unique<ChoiceDialog>(m_app.tr("Web interface"),
			QStringList{ m_app.tr("This changes the web interface that the TUI is connected through."), m_app.tr("The connection will probably drop and has to be set up again.") },
			QVector<Choice>{ { "S", m_app.tr("Save anyway"), 1 }, { "C", m_app.tr("Cancel"), -1 } },
			[this, alive, patch](int r) {
				if (r == 1 && !alive.expired())
					send(patch);
			}));
		return;
	}
	send(patch);
}

void SettingsDialog::send(const QJsonObject& patch)
{
	m_saving = true;
	std::weak_ptr<bool> alive = m_alive;
	App* app = &m_app;
	m_app.api().patchSettings(patch, [this, alive, app](const ApiResult& r) {
		if (!r.ok())
		{
			if (alive.expired())
				return app->toast(app->tr("Cannot save the settings: %1").arg(r.describe()), true);
			m_saving = false;
			m_error = r.describe();
			return;
		}
		app->api().applySettings(app->reply(app->tr("Cannot apply the settings")));
		app->toast(app->tr("Settings saved"));
		if (!alive.expired())
			close();
	});
}

void SettingsDialog::generateCertificate()
{
	Page* p = page();
	if (!p || !p->panel.field("remote/ssl_pem"))
		return;
	const Endpoint& ep = m_app.api().http()->endpoint();
	const QString host = ep.kind == Endpoint::Unix || ep.host.isEmpty() ? QStringLiteral("localhost") : ep.host;
	std::weak_ptr<bool> alive = m_alive;
	App* app = &m_app;
	dialogs::prompt(m_app, m_app.tr("Generate a self-signed certificate"), m_app.tr("Hostname"), host, m_app.tr("Generate"),
		[this, alive, app](const QString& hostname) {
			app->api().generateCertificate(hostname.trimmed(), [this, alive, app](const ApiResult& r) {
				if (!r.ok())
					return app->toast(app->tr("Cannot generate a certificate: %1").arg(r.describe()), true);
				const QString path = r.json.toObject()["path"].toString();
				if (alive.expired())
					return app->toast(app->tr("Certificate generated: %1").arg(path));
				for (auto& pg : m_pages)
					if (pg->script == "/scripts/settings/webinterface.js")
						pg->panel.setText("remote/ssl_pem", path);
				app->toast(app->tr("Certificate generated: %1 (press OK to use it)").arg(path));
			});
		});
}

// ------------------------------------------------------------------ input

QVector<Hint> SettingsDialog::hints()
{
	QVector<Hint> h;
	const QString enter = QString::fromUtf8("⏎");
	if (m_state != State::Ready)
	{
		h.push_back({ "Esc", m_app.tr("Close"), [this]() { close(); } });
		return h;
	}
	if (!m_formFocus)
	{
		h.push_back({ enter, m_app.tr("Edit"), [this]() { m_formFocus = true; } });
		h.push_back({ "S", m_app.tr("Save"), [this]() { save(); } });
		h.push_back({ "Esc", m_app.tr("Cancel"), [this]() { requestCancel(); } });
		return h;
	}
	h.push_back({ enter, m_app.tr("Save"), [this]() { save(); } });
	h.push_back({ "Tab", m_app.tr("Next"), [this]() { if (Page* p = page()) p->panel.moveFocus(1); } });
	h.push_back({ "PgDn", m_app.tr("Next page"), [this]() { selectRelative(1); } });
	if (Page* p = page(); p && p->script == "/scripts/settings/webinterface.js")
		h.push_back({ "^G", m_app.tr("Certificate"), [this]() { generateCertificate(); } });
	h.push_back({ "Esc", compact() ? m_app.tr("Back") : m_app.tr("Pages"), [this]() { m_formFocus = false; } });
	return h;
}

void SettingsDialog::handleListKey(const Event& e)
{
	if (e.is(KeyEscape))
		return requestCancel();
	if (e.is(KeyUp) || e.is(KeyPageUp))
		return selectRelative(-1);
	if (e.is(KeyDown) || e.is(KeyPageDown))
		return selectRelative(1);
	if (e.is(KeyHome))
		return select(0);
	if (e.is(KeyEnd))
		return select(int(m_pages.size()) - 1);
	if (e.is(KeyEnter) || e.is(KeyRight) || e.is(KeyTab))
		m_formFocus = true;
	else if (e.isLetter('s'))
		save();
}

void SettingsDialog::handleFormKey(const Event& e)
{
	Page* p = page();
	if (!p)
		return;
	if (e.is(KeyEscape))
	{
		m_formFocus = false;
		return;
	}
	if (e.is(KeyEnter))
		return save();
	if (e.is(KeyPageUp))
		return selectRelative(-1);
	if (e.is(KeyPageDown))
		return selectRelative(1);
	if (e.ctrl && e.key < 0x200000 && QChar(e.key).toLower() == QChar('g') && p->script == "/scripts/settings/webinterface.js")
		return generateCertificate();
	// leaving the first or the last field goes back to the page list
	if (e.is(KeyTab) && p->panel.focusAtEdge(1))
	{
		m_formFocus = false;
		return;
	}
	if (e.is(KeyBackTab) && p->panel.focusAtEdge(-1))
	{
		m_formFocus = false;
		return;
	}
	m_error.clear();
	p->panel.key(e);
}

void SettingsDialog::handle(const Event& e)
{
	if (m_saving)
		return;

	if (e.type == Event::Mouse)
	{
		Page* p = page();
		if (m_state == State::Ready && p && m_formFocus && m_rect.contains(e.x, e.y) && p->panel.mouse(e))
			return;
		handleMouse(e);
		return;
	}
	if (e.type == Event::Paste)
	{
		if (m_state == State::Ready && m_formFocus)
			if (Page* p = page())
				p->panel.paste(e);
		return;
	}
	if (e.type != Event::Key)
		return;

	if (m_state != State::Ready)
	{
		if (e.is(KeyEscape) || e.is(KeyEnter))
			close();
		return;
	}
	if (m_formFocus)
		handleFormKey(e);
	else
		handleListKey(e);
}

// ------------------------------------------------------------------ drawing

void SettingsDialog::draw(Canvas& c, HitMap& hits, const Rect& screen)
{
	beginDraw(hits, screen);
	const bool narrow = compact();
	const int w = narrow ? screen.w : qMin(screen.w - 4, 92);
	const int h = narrow ? screen.h : qMin(screen.h - 2, 30);
	m_rect = place(screen, w, h);

	QString title = m_app.tr("Settings");
	if (narrow && m_formFocus && page())
		title += " > " + page()->title;
	drawFrame(c, title);
	c.setCursor(0, 0, false);

	const int innerW = m_rect.w - 4;
	const Rect body = { m_rect.x + 2, m_rect.y + 2, innerW, m_rect.h - 5 };

	if (m_state == State::Loading)
		c.put(body.x, body.y, m_app.tr("Loading..."), Style::Dim);
	else if (m_state == State::Failed)
	{
		c.put(body.x, body.y, fit(m_app.tr("Cannot load the settings"), innerW), Style::Red);
		c.put(body.x, body.y + 1, fit(m_error, innerW), Style::Dim);
	}
	else
	{
		const int listW = 20;
		const bool showList = !narrow || !m_formFocus;
		const bool showForm = !narrow || m_formFocus;
		int formX = body.x, formW = body.w;

		if (showList)
		{
			const int lw = narrow ? body.w : listW;
			for (int i = 0; i < int(m_pages.size()) && i < body.h; i++)
			{
				const bool sel = i == m_sel;
				const Style st = sel ? (m_formFocus ? Style::Hl : Style::Selected) : Style::Normal;
				c.put(body.x, body.y + i, fit(" " + m_pages[i]->title, lw), st);
				addClick({ body.x, body.y + i, lw, 1 }, [this, i, narrow]() {
					select(i);
					m_formFocus = narrow;
				});
			}
			if (!narrow)
			{
				c.vline(body.x + listW + 1, body.y, body.h, Style::Border);
				formX = body.x + listW + 3;
				formW = body.w - listW - 3;
			}
		}
		if (showForm)
			if (Page* p = page())
				p->panel.draw(c, { formX, body.y, formW, body.h }, m_formFocus);
	}

	// error line and the hints
	if (m_saving)
		c.put(body.x, m_rect.bottom() - 3, m_app.tr("Saving..."), Style::Dim);
	else if (!m_error.isEmpty() && m_state == State::Ready)
		c.put(body.x, m_rect.bottom() - 3, fit(m_error, innerW), Style::Red);
	drawHints(c, m_rect.x + 1, m_rect.bottom() - 2, m_rect.w - 2, hints());
}

}
