#pragma once
#include "ApiClient.h"
#include "Dialog.h"
#include "Store.h"
#include <QObject>
#include <QTimer>
#include <memory>

namespace tui
{
	class App;
	class DetailsView;
	class ServerManager;

	enum HitTag
	{
		TagNone = 0, TagTab, TagBar, TagQueueRow, TagTransferRow, TagScroll, TagFileRow, TagDetailsTab,
		TagFeedRow, TagRuleRow, TagLogArea, TagQueueSwitch, TagPane,
	};

	class View
	{
	public:
		explicit View(App& app) : m_app(app) {}
		virtual ~View() = default;
		virtual Context context() const = 0;
		// area excludes the title bar, the tabs, the status line and the key bar
		virtual void draw(Canvas& c, HitMap& hits, const Rect& area) = 0;
		virtual void perform(Action a) = 0;
		virtual void mouse(const Event& e, const Hit* hit) { Q_UNUSED(e); Q_UNUSED(hit); }
		// Called about once a second while the view is shown
		virtual void poll() {}
		virtual void activated() {}
		virtual QString statusText() const { return QString(); }
		virtual QVector<BarItem> bar() const;

	protected:
		App& m_app;
	};

	class App : public QObject
	{
		Q_OBJECT
	public:
		enum class Tab { Transfers, Rss, Log };
		enum class Conn { Connecting, Connected, Disconnected, AuthFailed };

		App(ApiClient* api, QObject* parent = nullptr);
		~App() override;

		Store& store() { return m_store; }
		ApiClient& api() { return *m_api; }

		// ---- rendering and input
		void draw(Canvas& c);
		void handle(const Event& e);
		const HitMap& hits() const { return m_hits; }

		// ---- services for views and dialogs
		void push(std::unique_ptr<Dialog> d);
		bool hasDialog() const { return !m_dialogs.empty(); }
		void dismiss(Dialog* d);
		void toast(const QString& msg, bool error = false);
		// A callback for API calls whose only visible effect is an error message
		ApiClient::Callback reply(const QString& what, std::function<void()> onOk = nullptr);
		void refreshSoon();            // poll again at once
		void refreshQueues();
		void showTab(Tab t);
		void openDetails(const QString& transferUuid);
		void closeDetails();
		void showHelp();
		void showSettings();
		// The global speed limits (bytes per second, 0 = none); unknown until the server has answered
		bool limitsKnown() const { return m_limitsKnown; }
		SpeedPair globalLimits() const { return m_limits; }
		void setGlobalLimits(const SpeedPair& l) { m_limits = l; m_limitsKnown = true; }
		void refreshLimits();
		void askGlobalLimits();
		void askFilter();
		void askPassword(const QString& message = QString());
		void askCertificate(const QString& subject, const QString& fingerprint, const QStringList& problems);
		void requestQuit();
		// Quits at once without asking (signals, a vanished terminal): the server it started is only stopped if it is idle
		void quitNow();
		void setServer(ServerManager* s) { m_server = s; }
		void setDescription(const QString& d) { m_description = d; }
		void setPaused(bool p) { m_paused = p; }
		void setConnected();           // used by the startup code after the first successful call
		Conn connection() const { return m_conn; }
		Tab tab() const { return m_tab; }
		View* currentView();
		qint64 now() const;

		// Called by main() with the password the user typed; retries the connection
		std::function<void(const QString&)> passwordEntered;
		std::function<void(bool remember)> certificateDecision;

	signals:
		void quit();

	private slots:
		void tick();

	private:
		void perform(Action a);
		void onMouse(const Event& e);
		void drawChrome(Canvas& c, const Rect& area);
		void applyConnectionResult(const ApiResult& r);
		void finishQuit(bool stopServer);
		bool anythingActive() const;
		void pruneDialogs();

		ApiClient* m_api;
		Store m_store;
		HitMap m_hits;
		QTimer m_timer;
		std::vector<std::unique_ptr<Dialog>> m_dialogs;
		std::unique_ptr<View> m_transfers, m_rss, m_log;
		std::unique_ptr<DetailsView> m_details;
		Tab m_tab = Tab::Transfers;
		Conn m_conn = Conn::Connecting;
		QString m_message, m_description;
		bool m_messageError = false;
		qint64 m_messageUntil = 0;
		int m_ticks = 0, m_failures = 0;
		bool m_queuesDirty = true, m_inFlight = false, m_askingPassword = false, m_paused = false;
		ServerManager* m_server = nullptr;
		SpeedPair m_limits;
		bool m_limitsKnown = false;
		Rect m_screen;
	};
}
