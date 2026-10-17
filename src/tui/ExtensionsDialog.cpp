#include "ExtensionsDialog.h"
#include "Dialogs.h"
#include "Format.h"
#include <QJsonArray>
#include <QJsonObject>

namespace tui
{

ExtensionsDialog::ExtensionsDialog(App& app) : m_app(app)
{
	reload();
}

ExtensionsDialog::~ExtensionsDialog()
{
	*m_alive = false;
}

// ------------------------------------------------------------------ loading

// A server built without the Python extension system has no /plugins routes at
// all, which is not an error worth showing as "No such resource".
static QString describeFailure(App& app, const ApiResult& r)
{
	if (r.status == 404 && r.errorCode == "unknown_route")
		return app.tr("This FatRat server was built without the Python extension system (WITH_PYPLUGINS).");
	return r.describe();
}

void ExtensionsDialog::reload()
{
	std::weak_ptr<bool> alive = m_alive;

	m_pending = 3;
	m_error.clear();

	m_app.api().get("/plugins", [this, alive](const ApiResult& r) {
		if (alive.expired())
			return;
		if (!r.ok())
			return fail(describeFailure(m_app, r));

		m_installed.clear();
		for (const QJsonValue& v : r.json.toArray())
		{
			const QJsonObject o = v.toObject();
			Row row;

			row.id = o["name"].toString();
			row.valid = o["valid"].toBool();
			row.enabled = o["enabled"].toBool();
			row.title = row.id;
			if (!o["version"].toString().isEmpty())
				row.title += " " + o["version"].toString();
			// A plugin FatRat could not read is shown with its error: one that
			// quietly does not appear is the worst kind.
			row.detail = row.valid ? o["description"].toString() : o["error"].toString();
			// Loaded and unloaded are what the checkbox says already.
			if (o["state"].toString() == "draining")
				row.detail += "  [" + m_app.tr("%1 transfers still use it").arg(o["activeTransfers"].toInt()) + "]";
			else if (o["state"].toString() == "failed")
				row.detail += "  [" + m_app.tr("failed to load: %1").arg(o["loadError"].toString()) + "]";

			m_installed.push_back(row);
		}
		loaded();
	});

	m_app.api().get("/plugins/available", [this, alive](const ApiResult& r) {
		if (alive.expired())
			return;
		if (!r.ok())
			return fail(describeFailure(m_app, r));

		m_available.clear();
		for (const QJsonValue& v : r.json.toArray())
		{
			const QJsonObject o = v.toObject();
			Row row;

			row.id = o["name"].toString();
			row.repository = o["repository"].toString();
			row.title = row.id + " " + o["version"].toString();

			const QString installed = o["installedVersion"].toString();
			row.update = !installed.isEmpty() && installed != o["version"].toString();

			row.detail = o["description"].toString();
			if (row.update)
				row.detail = m_app.tr("installed: %1").arg(installed) + " - " + row.detail;
			else if (!installed.isEmpty())
				row.detail = m_app.tr("installed") + " - " + row.detail;

			for (const QJsonValue& d : o["dependencies"].toArray())
				row.dependencies << d.toString();

			m_available.push_back(row);
		}
		loaded();
	});

	m_app.api().get("/plugin-repositories", [this, alive](const ApiResult& r) {
		if (alive.expired())
			return;
		if (!r.ok())
			return fail(describeFailure(m_app, r));

		m_repositories.clear();
		for (const QJsonValue& v : r.json.toArray())
		{
			const QJsonObject o = v.toObject();
			Row row;

			row.id = o["id"].toString();
			row.title = o["name"].toString().isEmpty() ? row.id : o["name"].toString();
			row.detail = o["source"].toString();
			if (o["local"].toBool())
				row.detail += "  (" + m_app.tr("local directory") + ")";
			else if (!o["revision"].toString().isEmpty())
				row.detail += "  " + o["revision"].toString().left(10);

			m_repositories.push_back(row);
		}
		loaded();
	});
}

void ExtensionsDialog::loaded()
{
	if (--m_pending > 0)
		return;

	m_state = State::Ready;
	for (int i = 0; i < 3; i++)
		m_sel[i] = 0;
}

void ExtensionsDialog::fail(const QString& message)
{
	m_pending = 0;
	m_working = false;
	m_error = message;
	if (m_state == State::Loading)
		m_state = State::Failed;
}

ApiClient::Callback ExtensionsDialog::busy(const QString& what)
{
	std::weak_ptr<bool> alive = m_alive;

	m_working = true;
	m_error.clear();
	m_note.clear();

	return [this, alive, what](const ApiResult& r) {
		if (alive.expired())
			return;

		m_working = false;
		if (!r.ok())
			return fail(what + ": " + r.describe());

		reload();
	};
}

// ------------------------------------------------------------------ the rows

std::vector<ExtensionsDialog::Row>& ExtensionsDialog::rows()
{
	switch (m_section)
	{
	case Section::Available:    return m_available;
	case Section::Repositories: return m_repositories;
	default:                    return m_installed;
	}
}

const std::vector<ExtensionsDialog::Row>& ExtensionsDialog::rows() const
{
	return const_cast<ExtensionsDialog*>(this)->rows();
}

ExtensionsDialog::Row* ExtensionsDialog::current()
{
	const int sel = m_sel[int(m_section)];

	if (sel < 0 || sel >= int(rows().size()))
		return nullptr;
	return &rows()[sel];
}

void ExtensionsDialog::sectionRelative(int dir)
{
	int s = (int(m_section) + dir + 3) % 3;

	m_section = Section(s);
	m_note.clear();
}

void ExtensionsDialog::selectRelative(int dir)
{
	const int count = int(rows().size());

	if (!count)
		return;

	int& sel = m_sel[int(m_section)];
	sel = qBound(0, sel + dir, count - 1);
}

QString ExtensionsDialog::sectionTitle(Section s) const
{
	switch (s)
	{
	case Section::Available:    return m_app.tr("Available (%1)").arg(m_available.size());
	case Section::Repositories: return m_app.tr("Repositories (%1)").arg(m_repositories.size());
	default:                    return m_app.tr("Installed (%1)").arg(m_installed.size());
	}
}

// ------------------------------------------------------------------ actions

void ExtensionsDialog::toggleEnabled()
{
	Row* row = current();

	if (!row || !row->valid)
		return;

	const bool enabled = !row->enabled;
	QJsonObject body;

	body["enabled"] = enabled;
	m_app.api().call("PATCH", "/plugins/" + ApiClient::encode(row->id), body,
			 busy(m_app.tr("Cannot change %1").arg(row->id)));
	// A plugin that transfers still use stays loaded for them; the list says so.
	m_note = enabled ? m_app.tr("Enabled.")
			 : m_app.tr("Disabled. Transfers already using it carry on.");
}

void ExtensionsDialog::removePlugin()
{
	Row* row = current();

	if (!row)
		return;

	const QString name = row->id;
	QVector<Choice> choices = { { "Y", m_app.tr("Yes, remove it"), 0 }, { "N", m_app.tr("No"), -1 } };

	m_app.push(std::make_unique<ChoiceDialog>(m_app.tr("Remove plugin"),
		QStringList{ m_app.tr("Remove \"%1\"? Its files will be deleted.").arg(name) }, choices,
		[this, name](int r) {
			if (r < 0)
				return;
			m_app.api().call("DELETE", "/plugins/" + ApiClient::encode(name), QJsonValue(),
					 busy(m_app.tr("Cannot remove %1").arg(name)));
		}));
}

void ExtensionsDialog::installSelected()
{
	Row* row = current();

	if (!row)
		return;

	const QString name = row->id, repository = row->repository;
	const QStringList dependencies = row->dependencies;

	auto send = [this, name, repository](bool withDependencies) {
		QJsonObject body;

		body["repository"] = repository;
		body["plugin"] = name;
		if (withDependencies)
			body["withDependencies"] = true;

		m_app.api().call("POST", "/plugins", body, busy(m_app.tr("Cannot install %1").arg(name)));
		m_note = m_app.tr("%1 installed.").arg(name);
	};

	if (dependencies.isEmpty())
		return send(false);

	// Packages from PyPI are code the user did not choose, so they are named and
	// agreed to separately from the plugin itself.
	QStringList message;

	message << m_app.tr("%1 needs these packages from the Python Package Index:").arg(name);
	message << QString();
	for (const QString& dependency : dependencies)
		message << "  " + dependency;
	message << QString();
	message << m_app.tr("They will be downloaded with pip, for this plugin only.");

	QVector<Choice> choices = { { "Y", m_app.tr("Install them"), 0 }, { "N", m_app.tr("Cancel"), -1 } };

	m_app.push(std::make_unique<ChoiceDialog>(m_app.tr("Install dependencies"), message, choices,
		[send](int r) {
			if (r >= 0)
				send(true);
		}));
}

void ExtensionsDialog::addRepository()
{
	dialogs::prompt(m_app, m_app.tr("Add plugin repository"),
			m_app.tr("git URL, or a directory on the server"), QString(), m_app.tr("Add"),
		[this](const QString& source) {
			const QString trimmed = source.trimmed();

			if (trimmed.isEmpty())
				return;

			// The moment of trust: say what is at stake before anything is fetched.
			QStringList message;

			message << m_app.tr("Add this plugin repository?");
			message << QString();
			message << "  " + trimmed;
			message << QString();
			message << m_app.tr("Plugins you install from it run with the full privileges of the");
			message << m_app.tr("user running FatRat, like anything else installed from a git");
			message << m_app.tr("repository. Only add repositories you trust.");

			QVector<Choice> choices = { { "Y", m_app.tr("Add it"), 0 }, { "N", m_app.tr("Cancel"), -1 } };

			m_app.push(std::make_unique<ChoiceDialog>(m_app.tr("Add plugin repository"), message, choices,
				[this, trimmed](int r) {
					if (r < 0)
						return;

					QJsonObject body;

					body["source"] = trimmed;
					m_app.api().call("POST", "/plugin-repositories", body,
							 busy(m_app.tr("Cannot add the repository")));
				}));
		});
}

void ExtensionsDialog::forgetRepository()
{
	Row* row = current();

	if (!row)
		return;

	const QString id = row->id, title = row->title;
	QVector<Choice> choices = { { "Y", m_app.tr("Yes, forget it"), 0 }, { "N", m_app.tr("No"), -1 } };

	m_app.push(std::make_unique<ChoiceDialog>(m_app.tr("Forget repository"),
		QStringList{ m_app.tr("Forget \"%1\"? Plugins already installed from it stay installed.").arg(title) },
		choices,
		[this, id](int r) {
			if (r < 0)
				return;
			m_app.api().call("DELETE", "/plugin-repositories/" + ApiClient::encode(id), QJsonValue(),
					 busy(m_app.tr("Cannot forget the repository")));
		}));
}

void ExtensionsDialog::refreshRepositories()
{
	if (m_repositories.empty())
	{
		m_note = m_app.tr("No repositories to refresh. Press A to add one.");
		return;
	}

	// Fetching is git over the network; the dialog stays usable meanwhile.
	m_app.api().call("POST", "/plugin-repositories/refresh", QJsonObject(),
			 busy(m_app.tr("Cannot refresh the repositories")));
}

// ------------------------------------------------------------------ drawing

QVector<Hint> ExtensionsDialog::hints()
{
	QVector<Hint> h;

	h << Hint{ m_app.tr("Tab"), m_app.tr("Section"), [this]() { sectionRelative(1); } };

	switch (m_section)
	{
	case Section::Installed:
		h << Hint{ "E", m_app.tr("Enable/disable"), [this]() { toggleEnabled(); } };
		h << Hint{ "D", m_app.tr("Remove"), [this]() { removePlugin(); } };
		break;
	case Section::Available:
		h << Hint{ "⏎", m_app.tr("Install"), [this]() { installSelected(); } };
		break;
	case Section::Repositories:
		h << Hint{ "A", m_app.tr("Add"), [this]() { addRepository(); } };
		h << Hint{ "D", m_app.tr("Forget"), [this]() { forgetRepository(); } };
		h << Hint{ "R", m_app.tr("Refresh"), [this]() { refreshRepositories(); } };
		break;
	}

	h << Hint{ m_app.tr("Esc"), m_app.tr("Close"), [this]() { close(); } };
	return h;
}

void ExtensionsDialog::draw(Canvas& c, HitMap& hits, const Rect& screen)
{
	beginDraw(hits, screen);

	const int w = qMin(screen.w - 4, 92);
	const int h = qMin(screen.h - 2, 26);

	m_rect = place(screen, w, h);
	drawFrame(c, m_app.tr("Extensions"));
	c.setCursor(0, 0, false);

	const int innerW = m_rect.w - 4;
	const Rect body = { m_rect.x + 2, m_rect.y + 2, innerW, m_rect.h - 6 };

	if (m_state == State::Loading)
	{
		c.put(body.x, body.y, m_app.tr("Loading..."), Style::Dim);
		drawHints(c, m_rect.x + 1, m_rect.bottom() - 2, m_rect.w - 2, hints());
		return;
	}
	if (m_state == State::Failed)
	{
		c.put(body.x, body.y, fit(m_app.tr("Cannot load the extensions"), innerW), Style::Red);
		c.put(body.x, body.y + 1, fit(m_error, innerW), Style::Dim);
		drawHints(c, m_rect.x + 1, m_rect.bottom() - 2, m_rect.w - 2, hints());
		return;
	}

	// the three sections, as a row of clickable headings
	int x = body.x;

	for (int i = 0; i < 3; i++)
	{
		const Section s = Section(i);
		const bool active = s == m_section;
		// Brackets, not just a highlight: which section is open has to be
		// readable on a terminal that cannot show styles.
		const QString title = active ? "[" + sectionTitle(s) + "]" : " " + sectionTitle(s) + " ";

		c.put(x, body.y, title, active ? Style::Selected : Style::Normal);
		addClick({ x, body.y, title.size(), 1 }, [this, s]() { m_section = s; m_note.clear(); });
		x += title.size() + 1;
	}

	const Rect list = { body.x, body.y + 2, body.w, body.h - 2 };
	const std::vector<Row>& items = rows();
	const int sel = m_sel[int(m_section)];

	if (items.empty())
	{
		QString empty;

		switch (m_section)
		{
		case Section::Installed:
			empty = m_app.tr("No plugins installed.");
			break;
		case Section::Available:
			empty = m_repositories.empty() ? m_app.tr("Add a repository first (Tab, then A).")
						       : m_app.tr("The repositories offer nothing.");
			break;
		case Section::Repositories:
			empty = m_app.tr("No repositories added. Press A to add one.");
			break;
		}

		c.put(list.x, list.y, fit(empty, list.w), Style::Dim);
	}

	// Keep the selected row on screen without a scrollbar: a window of list.h rows.
	int top = 0;

	if (int(items.size()) > list.h)
		top = qBound(0, sel - list.h / 2, int(items.size()) - list.h);

	for (int i = top; i < int(items.size()) && i - top < list.h; i++)
	{
		const Row& row = items[i];
		const int y = list.y + i - top;
		const bool selected = i == sel;

		QString mark;

		if (m_section == Section::Installed)
			mark = row.valid ? (row.enabled ? "[x] " : "[ ] ") : "[!] ";
		else if (m_section == Section::Available && row.update)
			mark = "^ ";
		else
			mark = "  ";

		const int titleW = qMin(34, list.w / 2);
		const QString text = fit(mark + row.title, titleW) + " " + fit(row.detail, list.w - titleW - 1);

		c.put(list.x, y, fit(text, list.w), selected ? Style::Selected
							     : (row.valid ? Style::Normal : Style::Red));
		addClick({ list.x, y, list.w, 1 }, [this, i]() { m_sel[int(m_section)] = i; });
	}

	// one status line: what went wrong, or what will happen next
	const int statusY = m_rect.bottom() - 3;

	if (m_working)
		c.put(body.x, statusY, m_app.tr("Working..."), Style::Dim);
	else if (!m_error.isEmpty())
		c.put(body.x, statusY, fit(m_error, innerW), Style::Red);
	else if (!m_note.isEmpty())
		c.put(body.x, statusY, fit(m_note, innerW), Style::Dim);

	drawHints(c, m_rect.x + 1, m_rect.bottom() - 2, m_rect.w - 2, hints());
}

void ExtensionsDialog::handle(const Event& e)
{
	if (handleMouse(e))
		return;

	if (e.is(KeyEscape) || e.isLetter('q'))
		return close();

	if (e.is(KeyTab) || e.is(KeyRight))
		return sectionRelative(1);
	if (e.is(KeyBackTab) || e.is(KeyLeft))
		return sectionRelative(-1);

	if (e.is(KeyUp))
		return selectRelative(-1);
	if (e.is(KeyDown))
		return selectRelative(1);
	if (e.is(KeyPageUp))
		return selectRelative(-10);
	if (e.is(KeyPageDown))
		return selectRelative(10);
	if (e.is(KeyHome))
		return selectRelative(-int(rows().size()));
	if (e.is(KeyEnd))
		return selectRelative(int(rows().size()));

	if (m_state != State::Ready)
		return;

	switch (m_section)
	{
	case Section::Installed:
		if (e.isLetter('e') || e.is(KeyEnter) || e.isLetter(' '))
			return toggleEnabled();
		if (e.isLetter('d'))
			return removePlugin();
		break;
	case Section::Available:
		if (e.is(KeyEnter) || e.isLetter('i'))
			return installSelected();
		break;
	case Section::Repositories:
		if (e.isLetter('a'))
			return addRepository();
		if (e.isLetter('d'))
			return forgetRepository();
		if (e.isLetter('r'))
			return refreshRepositories();
		break;
	}
}

}
