#pragma once
#include "App.h"
#include "ListNav.h"
#include <QJsonObject>
#include <QMap>

namespace tui
{
	class DetailsView : public View
	{
	public:
		enum Tab { Info, Files, Log };

		DetailsView(App& app, const QString& uuid);
		Context context() const override { return m_tab == Files ? Context::Files : Context::Details; }
		void draw(Canvas& c, HitMap& hits, const Rect& area) override;
		void perform(Action a) override;
		void mouse(const Event& e, const Hit* hit) override;
		void poll() override;
		void activated() override { poll(); }
		QVector<BarItem> bar() const override;
		QString statusText() const override;

		bool hasFiles() const { return m_info["files"].isArray(); }
		Tab tab() const { return m_tab; }

	private:
		void drawInfo(Canvas& c, const Rect& r);
		void drawFiles(Canvas& c, HitMap& hits, const Rect& r);
		void drawLog(Canvas& c, const Rect& r);
		void changePriority(int index, int delta);
		void setPriority(int index, int prio);
		int priorityOf(int index) const;
		void apply();
		void nextTab(int dir);

		QString m_uuid;
		TransferInfo m_t;
		bool m_haveTransfer = false;
		QJsonObject m_info;
		QVector<SpeedPair> m_speeds;
		QStringList m_log;
		Tab m_tab = Info;
		ListNav m_files, m_logNav;
		int m_rows = 10;
		QMap<int, int> m_pending;     // file index -> priority not applied yet
		int m_gen = 0;
	};
}
