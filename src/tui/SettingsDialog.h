#pragma once
#include "App.h"
#include "SchedulerPanel.h"
#include "SettingsPages.h"
#include <memory>
#include <vector>

namespace tui
{
	// The settings window: a list of pages and the form of the selected one, like the web interface's dialog.
	// Edits stay in the forms of all pages until OK, which sends everything that changed in one request.
	class SettingsDialog : public Dialog
	{
	public:
		explicit SettingsDialog(App& app);
		~SettingsDialog() override;

		void draw(Canvas& c, HitMap& hits, const Rect& screen) override;
		void handle(const Event& e) override;
		// A stray click must not throw away what was typed
		void outsideClick() override {}

	private:
		struct Page
		{
			QString title, script;
			// Points at `ownSpec` for a page built from server data
			// (Extensions), at a static spec otherwise.
			const SettingsPageSpec* spec = nullptr;
			SettingsPageSpec ownSpec;
			QJsonObject loaded;
			FormPanel panel;
			// Set for the Scheduler page, which is a list edited in place rather than a form
			std::unique_ptr<SchedulerPanel> scheduler;
		};
		enum class State { Loading, Ready, Failed };

		void loadPages();
		// The Extensions page's contents depend on what is installed, so
		// its spec is built after fetching GET /plugins.
		void loadPlugins(const QStringList& keys);
		void loadValues(const QStringList& keys);
		void buildPages(const QJsonObject& values);
		void fail(const QString& msg);

		bool compact() const { return m_screen.w < 100; }
		Page* page() { return m_sel >= 0 && m_sel < int(m_pages.size()) ? m_pages[m_sel].get() : nullptr; }
		void select(int index);
		void selectRelative(int dir);
		bool dirty() const;
		void requestCancel();
		void save();
		bool collect(QJsonObject& patch);
		void send(const QJsonObject& patch);
		void generateCertificate();
		QVector<Hint> hints();
		void handleListKey(const Event& e);
		void handleFormKey(const Event& e);

		App& m_app;
		State m_state = State::Loading;
		QString m_error;
		std::vector<std::unique_ptr<Page>> m_pages;
		int m_sel = 0;
		bool m_formFocus = false;    // else the page list; in the compact layout this means "the page is open"
		bool m_saving = false;
		std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
	};
}
