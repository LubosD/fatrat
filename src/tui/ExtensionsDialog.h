#pragma once
#include "App.h"
#include <QJsonArray>
#include <QStringList>
#include <memory>
#include <vector>

namespace tui
{
	/* Extensions: the plugin repositories the user has added, what they offer, and what is
	 * installed. The same operations as the desktop settings page and fatrat-conf, so a FatRat
	 * driven from a terminal is not a lesser one.
	 *
	 * Everything here is one REST call; none of them block the screen, because a repository
	 * refresh is a git fetch and takes as long as it takes. The two questions that matter --
	 * do you trust this repository, and may these PyPI packages be installed -- are asked
	 * before anything is fetched, naming exactly what is at stake.
	 */
	class ExtensionsDialog : public Dialog
	{
	public:
		explicit ExtensionsDialog(App& app);
		~ExtensionsDialog() override;

		void draw(Canvas& c, HitMap& hits, const Rect& screen) override;
		void handle(const Event& e) override;
		// A stray click must not close a window the user is working in
		void outsideClick() override {}

	private:
		enum class Section { Installed, Available, Repositories };
		enum class State { Loading, Ready, Failed };

		struct Row
		{
			QString id;        // plugin name, or repository id
			QString title;     // what the row says
			QString detail;    // the dimmer second column
			QString repository;
			bool enabled = true;
			bool valid = true;
			bool update = false;   // Available: a newer version than the installed one
			QStringList dependencies;
		};

		void reload();
		void loaded();
		void fail(const QString& message);

		std::vector<Row>& rows();
		const std::vector<Row>& rows() const;
		Row* current();

		void sectionRelative(int dir);
		void selectRelative(int dir);

		// ---- what the keys do
		void toggleEnabled();
		void removePlugin();
		void installSelected();
		void addRepository();
		void forgetRepository();
		void refreshRepositories();

		// Runs `call`, keeping the dialog responsive and showing failures in it rather than
		// as a toast the user may not connect to what they just did.
		ApiClient::Callback busy(const QString& what);

		QVector<Hint> hints();
		QString sectionTitle(Section s) const;

		App& m_app;
		State m_state = State::Loading;
		QString m_error, m_note;
		Section m_section = Section::Installed;
		int m_sel[3] = { 0, 0, 0 };
		int m_pending = 0;          // outstanding load requests
		bool m_working = false;     // a call the user started is in flight

		std::vector<Row> m_installed, m_available, m_repositories;
		std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
	};
}
