#pragma once
#include "App.h"
#include "ListNav.h"

namespace tui
{
	class LogView : public View
	{
	public:
		explicit LogView(App& app) : View(app) {}
		Context context() const override { return Context::Log; }
		void draw(Canvas& c, HitMap& hits, const Rect& area) override;
		void perform(Action a) override;
		void mouse(const Event& e, const Hit* hit) override;
		void poll() override;
		void activated() override { m_follow = true; poll(); }
		QString statusText() const override;
		QVector<BarItem> bar() const override;

		void setLines(const QStringList& l) { m_lines = l; }

	private:
		void search(bool forward);

		QStringList m_lines;
		QString m_transferUuid, m_transferName, m_needle;
		int m_top = 0, m_rows = 10;
		bool m_follow = true;
		int m_gen = 0, m_applied = 0;
	};
}
