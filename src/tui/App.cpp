#include "App.h"
#include "DetailsView.h"
#include "Dialogs.h"
#include "Format.h"
#include "LogView.h"
#include "RssView.h"
#include "ServerManager.h"
#include "TransfersView.h"
#include <QDateTime>
#include <algorithm>

namespace tui
{

QVector<BarItem> View::bar() const
{
	return Keymap::bar(context());
}

App::App(ApiClient* api, QObject* parent) : QObject(parent), m_api(api)
{
	m_transfers.reset(new TransfersView(*this));
	m_rss.reset(new RssView(*this));
	m_log.reset(new LogView(*this));
	m_timer.setInterval(1000);
	connect(&m_timer, &QTimer::timeout, this, &App::tick);
	m_timer.start();
	QTimer::singleShot(0, this, &App::tick);
}

App::~App() = default;

qint64 App::now() const
{
	return QDateTime::currentMSecsSinceEpoch();
}

View* App::currentView()
{
	if (m_details)
		return m_details.get();
	switch (m_tab)
	{
		case Tab::Rss: return m_rss.get();
		case Tab::Log: return m_log.get();
		default: return m_transfers.get();
	}
}

void App::push(std::unique_ptr<Dialog> d)
{
	m_dialogs.push_back(std::move(d));
}

void App::pruneDialogs()
{
	m_dialogs.erase(std::remove_if(m_dialogs.begin(), m_dialogs.end(), [](const std::unique_ptr<Dialog>& d) { return d->finished(); }), m_dialogs.end());
}

void App::dismiss(Dialog* d)
{
	if (d)
		d->close();
	pruneDialogs();
}

void App::toast(const QString& msg, bool error)
{
	m_message = msg;
	m_messageError = error;
	m_messageUntil = now() + (error ? 8000 : 4000);
}

ApiClient::Callback App::reply(const QString& what, std::function<void()> onOk)
{
	return [this, what, onOk](const ApiResult& r) {
		if (r.ok())
		{
			if (onOk)
				onOk();
		}
		else
		{
			toast(what + ": " + r.describe(), true);
		}
		refreshSoon();
	};
}

void App::refreshSoon()
{
	QTimer::singleShot(0, this, &App::tick);
}

void App::refreshQueues()
{
	m_queuesDirty = true;
	refreshSoon();
}

void App::showTab(Tab t)
{
	m_details.reset();
	m_tab = t;
	currentView()->activated();
	refreshSoon();
}

void App::openDetails(const QString& uuid)
{
	m_details.reset(new DetailsView(*this, uuid));
	m_details->activated();
}

void App::closeDetails()
{
	// may be called from the view itself
	QTimer::singleShot(0, this, [this]() {
		m_details.reset();
		refreshSoon();
	});
}

void App::setConnected()
{
	m_conn = Conn::Connected;
	m_failures = 0;
}

// ------------------------------------------------------------------ polling

void App::applyConnectionResult(const ApiResult& r)
{
	if (r.ok())
	{
		m_conn = Conn::Connected;
		m_failures = 0;
		return;
	}
	if (r.unauthorized())
	{
		m_conn = Conn::AuthFailed;
		askPassword(m_askingPassword ? QString() : tr("Wrong password"));
		return;
	}
	if (r.transportFailure())
	{
		m_failures++;
		m_conn = Conn::Disconnected;
	}
}

void App::tick()
{
	m_ticks++;
	if (m_paused)
		return;
	HttpClient* http = m_api->http();
	if (http->waitingForCertificateDecision() || m_askingPassword)
		return;
	// do not pile requests up behind a slow server
	if (http->pendingCount() > 2)
		return;

	m_api->stats([this](const ApiResult& r) {
		applyConnectionResult(r);
		if (r.ok())
			m_store.setStats(Stats::fromJson(r.json.toObject()));
	});

	const bool wantQueues = m_queuesDirty || m_ticks % 5 == 0 || m_store.queues().isEmpty();
	if (wantQueues)
	{
		m_queuesDirty = false;
		m_api->queues([this](const ApiResult& r) {
			if (!r.ok())
				return;
			QVector<QueueInfo> q;
			for (const QJsonValue& v : r.json.toArray())
				q.push_back(QueueInfo::fromJson(v.toObject()));
			m_store.setQueues(q);
			// the counts of the other queues, for the queue list
			for (const QueueInfo& other : q)
				if (other.uuid != m_store.currentQueueUuid())
					m_api->queueTransfers(other.uuid, [this, uuid = other.uuid](const ApiResult& rr) {
						if (!rr.ok())
							return;
						int busy = 0;
						for (const QJsonValue& v : rr.json.toArray())
						{
							const TransferInfo t = TransferInfo::fromJson(v.toObject());
							busy += t.isActive() || t.state == TransferInfo::Waiting;
						}
						m_store.setQueueCount(uuid, rr.json.toArray().size(), busy);
					});
			if (m_tab == Tab::Transfers && !m_details)
				m_api->queueTransfers(m_store.currentQueueUuid(), [this](const ApiResult&) {});
		});
	}

	if (!m_store.currentQueueUuid().isEmpty())
	{
		const QString q = m_store.currentQueueUuid();
		m_api->queueTransfers(q, [this, q](const ApiResult& r) {
			if (!r.ok())
				return;
			QVector<TransferInfo> t;
			for (const QJsonValue& v : r.json.toArray())
				t.push_back(TransferInfo::fromJson(v.toObject()));
			m_store.setTransfers(q, t);
		});
	}

	currentView()->poll();
}

// -------------------------------------------------------------------- input

void App::handle(const Event& e)
{
	if (e.type == Event::Resize)
		return;

	if (!m_dialogs.empty())
	{
		m_dialogs.back()->handle(e);
		pruneDialogs();
		return;
	}

	if (e.type == Event::Mouse)
		return onMouse(e);
	if (e.type != Event::Key)
		return;

	perform(Keymap::lookup(currentView()->context(), e));
}

void App::onMouse(const Event& e)
{
	const Hit* hit = m_hits.at(e.x, e.y);
	if (e.mouse == Event::Press && e.button == 1 && hit)
	{
		if (hit->tag == TagTab)
		{
			showTab(hit->action == Action::TabRss ? Tab::Rss : hit->action == Action::TabLog ? Tab::Log : Tab::Transfers);
			return;
		}
		if (hit->tag == TagBar)
		{
			perform(hit->action);
			return;
		}
	}
	currentView()->mouse(e, hit);
}

void App::perform(Action a)
{
	switch (a)
	{
		case Action::None:
			return;
		case Action::TabTransfers: return showTab(Tab::Transfers);
		case Action::TabRss: return showTab(Tab::Rss);
		case Action::TabLog: return showTab(Tab::Log);
		case Action::Help: return showHelp();
		case Action::Quit: return requestQuit();
		case Action::Redraw: return;
		case Action::Filter:
			if (m_tab == Tab::Transfers && !m_details)
				askFilter();
			else
				currentView()->perform(a);
			return;
		default:
			currentView()->perform(a);
	}
}

// ---------------------------------------------------------------- dialogs

void App::askFilter()
{
	auto d = std::make_unique<FormDialog>(tr("Filter transfers"), 56);
	d->addText("filter", tr("Name contains"), m_store.filter());
	d->setSubmit(tr("Apply"), [this](FormDialog& f) {
		m_store.setFilter(f.text("filter").trimmed());
		return true;
	});
	push(std::move(d));
}

void App::askPassword(const QString& message)
{
	if (m_askingPassword)
		return;
	m_askingPassword = true;

	auto d = std::make_unique<FormDialog>(tr("Password required"), 58);
	d->addLabel(tr("Connecting to %1").arg(m_description));
	d->addSpacer();
	d->addText("pw", tr("Web interface password")).secret = true;
	d->setError(message);
	d->setSubmit(tr("Connect"), [this](FormDialog& f) {
		m_askingPassword = false;
		if (passwordEntered)
			passwordEntered(f.text("pw"));
		refreshSoon();
		return true;
	});
	d->setCancel([this]() {
		m_askingPassword = false;
		emit quit();
	});
	push(std::move(d));
}

void App::askCertificate(const QString& subject, const QString& fingerprint, const QStringList& problems)
{
	QStringList msg;
	msg << tr("The server certificate is not trusted:");
	for (const QString& p : problems)
		msg << "  " + p;
	msg << "";
	msg << tr("Subject  %1").arg(subject);
	msg << tr("SHA-256  %1").arg(fingerprint.left(fingerprint.size() / 2 + 1));
	msg << QString("        %1").arg(fingerprint.mid(fingerprint.size() / 2 + 1));
	QVector<Choice> ch = { { "T", tr("Trust for this session"), 0 }, { "R", tr("Remember this fingerprint"), 1 }, { "Q", tr("Quit"), -1 } };
	auto cert = std::make_unique<ChoiceDialog>(tr("Certificate"), msg, ch, [this](int r) {
		if (r < 0)
		{
			m_api->http()->rejectCertificate();
			emit quit();
			return;
		}
		m_api->http()->acceptCertificate();
		if (certificateDecision)
			certificateDecision(r == 1);
	});
	cert->setCancelOnOutside(false);
	push(std::move(cert));
}

void App::showHelp()
{
	struct Section { Context ctx; const char* name; };
	const Section sections[] = {
		{ Context::Global, "Everywhere" }, { Context::Transfers, "Transfers" }, { Context::Queues, "Queues" },
		{ Context::Details, "Details view" }, { Context::Files, "Torrent files" },
		{ Context::RssFeeds, "RSS feeds" }, { Context::RssRules, "RSS rules" }, { Context::Log, "Log" },
	};
	auto keyName = [](const Binding& b) {
		if (b.keyLabel)
			return QString::fromUtf8(b.keyLabel);
		switch (b.key)
		{
			case KeyUp: return QString::fromUtf8("↑");
			case KeyDown: return QString::fromUtf8("↓");
			case KeyLeft: return QString::fromUtf8("←");
			case KeyRight: return QString::fromUtf8("→");
			case KeyPageUp: return QString("PgUp");
			case KeyPageDown: return QString("PgDn");
			case KeyHome: return QString("Home");
			case KeyEnd: return QString("End");
			case KeyTab: return QString("Tab");
			case KeyBackTab: return QString("Shift-Tab");
			case KeyEscape: return QString("Esc");
			case KeyEnter: return QString::fromUtf8("⏎");
			case KeyF1: return QString("F1");
			default: break;
		}
		QString k = QString(QChar(b.key)).toUpper();
		return b.ctrl ? "Ctrl-" + k : k;
	};
	static const QMap<Action, const char*> names = {
		{ Action::TabTransfers, "Transfers tab" }, { Action::TabRss, "RSS tab" }, { Action::TabLog, "Log tab" },
		{ Action::FocusNext, "Next pane" }, { Action::FocusPrev, "Previous pane" }, { Action::Up, "Up" }, { Action::Down, "Down" },
		{ Action::Left, "Left (queues pane)" }, { Action::Right, "Right (transfers pane)" }, { Action::PageUp, "Page up" }, { Action::PageDown, "Page down" },
		{ Action::Home, "First" }, { Action::End, "Last" }, { Action::Filter, "Filter by name" }, { Action::Help, "This help" },
		{ Action::Quit, "Quit" }, { Action::Redraw, "Redraw the screen" }, { Action::Back, "Back / clear filter" },
		{ Action::Resume, "Resume" }, { Action::Force, "Force resume" }, { Action::Pause, "Pause" }, { Action::NewTransfer, "New transfer" },
		{ Action::DeleteTransfers, "Delete transfers" }, { Action::EditTransfer, "Edit properties" }, { Action::MoveMenu, "Move (then T U D B)" },
		{ Action::MoveUp, "Move up" }, { Action::MoveDown, "Move down" }, { Action::SendToQueue, "Send to another queue" },
		{ Action::LimitTransfer, "Speed limit" }, { Action::ToggleMark, "Mark / unmark" }, { Action::MarkAll, "Mark all / none" },
		{ Action::Details, "Open details" }, { Action::NewQueue, "New queue" }, { Action::EditQueue, "Edit queue" },
		{ Action::DeleteQueue, "Delete queue" }, { Action::QueueMoveUp, "Move queue up" }, { Action::QueueMoveDown, "Move queue down" },
		{ Action::LimitQueue, "Queue speed limits" }, { Action::OpenQueue, "Open queue" }, { Action::NextTab, "Next tab" },
		{ Action::PrevTab, "Previous tab" }, { Action::PrioUp, "Raise priority" }, { Action::PrioDown, "Lower priority" },
		{ Action::SkipFile, "Skip file" }, { Action::SelectAllFiles, "All files" }, { Action::ApplyFiles, "Apply priorities" },
		{ Action::NewItem, "New" }, { Action::EditItem, "Edit" }, { Action::DeleteItem, "Delete" }, { Action::RefreshFeeds, "Refresh feeds now" },
		{ Action::TestRegexp, "Test a regular expression" }, { Action::DownloadedList, "Downloaded episodes" }, { Action::Follow, "Follow the end" },
		{ Action::Search, "Search" }, { Action::TopOfLog, "Top" }, { Action::TransferLog, "Log of the selected transfer" },
		{ Action::SearchNext, "Next match" },
	};

	QStringList lines;
	for (const Section& s : sections)
	{
		if (!lines.isEmpty())
			lines << "";
		lines << QString("%1").arg(tr(s.name).toUpper());
		QMap<Action, QStringList> keys;
		QVector<Action> order;
		for (const Binding& b : Keymap::bindings())
			if (b.context == s.ctx)
			{
				if (!keys.contains(b.action))
					order << b.action;
				keys[b.action] << keyName(b);
			}
		for (Action a : order)
			lines << QString("  %1  %2").arg(keys[a].join(" / "), -14).arg(tr(names.value(a, "")));
	}
	lines << "" << tr("MOUSE") << tr("  click        select / press a button or a key hint")
		<< tr("  double-click open details / edit queue") << tr("  right-click  context menu")
		<< tr("  ctrl-click   mark a transfer, shift-click marks a range")
		<< tr("  drag         move a transfer, send it to a queue, reorder queues")
		<< tr("  wheel        scroll");
	push(std::make_unique<TextDialog>(tr("Keys"), lines, 78));
}

// --------------------------------------------------------------------- quit

bool App::anythingActive() const
{
	if (m_store.stats().total.down > 0 || m_store.stats().total.up > 0)
		return true;
	for (const TransferInfo& t : m_store.transfers())
		if (t.isActive() || t.state == TransferInfo::Waiting)
			return true;
	return m_store.otherQueuesBusy();
}

void App::requestQuit()
{
	if (m_server && m_server->startedByUs())
	{
		if (anythingActive())
		{
			QVector<Choice> ch = { { "K", tr("Keep it running (downloads continue)"), 0 }, { "S", tr("Stop the server and quit"), 1 }, { "C", tr("Cancel"), -1 } };
			push(std::make_unique<ChoiceDialog>(tr("Quit"), QStringList{ tr("This TUI started the FatRat server, and transfers are active.") }, ch,
				[this](int r) { if (r >= 0) finishQuit(r == 1); }));
			return;
		}
		return finishQuit(true);
	}
	finishQuit(false);
}

void App::quitNow()
{
	finishQuit(m_server && m_server->startedByUs() && !anythingActive());
}

void App::finishQuit(bool stopServer)
{
	if (stopServer && m_server)
		m_server->stop();
	emit quit();
}

// ---------------------------------------------------------------- drawing

void App::drawChrome(Canvas& c, const Rect& area)
{
	const int W = area.w, H = area.h;
	const Glyphs& g = Glyphs::get();

	// title bar
	QString state;
	Style titleStyle = Style::Title;
	switch (m_conn)
	{
		case Conn::Connecting: state = g.dot + tr(" connecting"); break;
		case Conn::Connected: state = g.dot + tr(" connected"); break;
		case Conn::Disconnected: state = g.dot + tr(" disconnected, retrying"); titleStyle = Style::Error; break;
		case Conn::AuthFailed: state = g.dot + tr(" password required"); titleStyle = Style::Error; break;
	}
	c.fill({ 0, 0, W, 1 }, titleStyle);
	const SpeedPair t = m_store.stats().total;
	const QString rates = g.down + " " + formatSpeed(t.down) + "   " + g.up + " " + formatSpeed(t.up) + " ";
	c.put(0, 0, fit(" FatRat   " + state + "  " + m_description, qMax(0, W - displayWidth(rates) - 1)), titleStyle);
	c.put(W - displayWidth(rates), 0, rates, titleStyle);

	// tabs
	struct T { Tab tab; Action a; const char* name; };
	const T tabs[] = { { Tab::Transfers, Action::TabTransfers, "1 Transfers" }, { Tab::Rss, Action::TabRss, "2 RSS" }, { Tab::Log, Action::TabLog, "3 Log" } };
	int x = 1;
	for (const T& tb : tabs)
	{
		const QString label = " " + tr(tb.name) + " ";
		const bool on = m_tab == tb.tab && !m_details;
		c.put(x, 1, label, on ? Style::TabOn : Style::Dim);
		m_hits.add({ x, 1, displayWidth(label), 1 }, tb.a, TagTab);
		x += displayWidth(label) + 2;
	}

	View* v = currentView();
	v->draw(c, m_hits, { 0, 2, W, H - 4 });

	// status line
	c.fill({ 0, H - 2, W, 1 }, Style::Status);
	if (!m_message.isEmpty() && now() < m_messageUntil)
	{
		c.put(1, H - 2, fit(m_message, W - 2), m_messageError ? Style::StatusWarn : Style::Status);
	}
	else
	{
		QString s = v->statusText();
		if (s.isEmpty())
		{
			const SpeedPair qs = m_store.queueSpeeds(m_store.currentQueueUuid());
			s = QString(" %1 %2 %3 %4").arg(g.down, formatSpeed(t.down), g.up, formatSpeed(t.up));
			if (const QueueInfo* q = m_store.currentQueue())
				s += QString(" │ %1 %2 %3 %4 %5").arg(q->name, g.down, formatSpeed(qs.down), g.up, formatSpeed(qs.up));
			if (!m_store.marks().isEmpty())
				s += tr(" │ %1 marked").arg(m_store.marks().size());
		}
		c.put(0, H - 2, fit(s, W), Style::Status);
	}

	// key bar
	c.fill({ 0, H - 1, W, 1 }, Style::KeyBar);
	int bx = 1;
	for (const BarItem& it : v->bar())
	{
		const int w = hintWidth(it.key, it.label);
		if (bx + w > W - 1)
			break;
		drawHint(c, bx, H - 1, it.key, it.label, Style::KeyBar, it.enabled);
		if (it.enabled)
			m_hits.add({ bx, H - 1, w, 1 }, it.action, TagBar);
		bx += w + 2;
	}
}

void App::draw(Canvas& c)
{
	const int W = c.width(), H = c.height();
	m_hits.clear();
	m_screen = { 0, 0, W, H };
	c.setCursor(0, 0, false);
	c.fill(m_screen, Style::Normal);

	if (W < 60 || H < 16)
	{
		c.put(1, 1, tr("Terminal too small (need 60x16)"), Style::Red);
		c.put(1, 2, QString("%1x%2").arg(W).arg(H), Style::Dim);
		return;
	}

	drawChrome(c, m_screen);
	for (auto& d : m_dialogs)
		d->draw(c, m_hits, m_screen);
}

}
