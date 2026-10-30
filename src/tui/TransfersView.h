#pragma once
#include "App.h"
#include <QJsonObject>

namespace tui
{
	class TransfersView : public View
	{
	public:
		explicit TransfersView(App& app) : View(app) {}
		Context context() const override { return m_focusQueues ? Context::Queues : Context::Transfers; }
		void draw(Canvas& c, HitMap& hits, const Rect& area) override;
		void perform(Action a) override;
		void mouse(const Event& e, const Hit* hit) override;
		void poll() override;
		QVector<BarItem> bar() const override;

		// For tests
		bool queuesFocused() const { return m_focusQueues; }
		void focusQueues(bool f) { m_focusQueues = f; }

	private:
		struct Columns { int name = 0, state = 10, progress = 15, size = 9, down = 10, eta = 7; bool showSize = true, showEta = true; };
		Columns columns(int width) const;
		void drawQueues(Canvas& c, HitMap& hits, const Rect& box);
		void drawTransfers(Canvas& c, HitMap& hits, const Rect& box);
		void drawDetails(Canvas& c, const Rect& r);
		void ensureVisible();
		void setState(const QString& state, TransferInfo::State local);
		void resume();
		void moveSelected(const QString& direction);
		void moveQueue(const QString& direction);
		void reorderQueue(int from, int to);
		void transferMenu(int x, int y);
		void queueMenu(int x, int y);
		void navigate(int delta, bool absolute = false);
		void drop();
		QStringList dragUuids() const;

		bool m_focusQueues = false;
		int m_scroll = 0;
		bool m_follow = true;
		Rect m_queueBox, m_listRect, m_scrollbar;
		int m_rows = 0, m_anchor = 0;
		bool m_compact = false;

		// pointer drag
		struct Press { bool down = false, active = false; int tag = 0, index = 0, x = 0, y = 0; QString uuid; } m_press;
		int m_dropRow = -1, m_dropQueue = -1;

		// details strip
		QString m_infoUuid;
		QJsonObject m_info;
		QVector<SpeedPair> m_speeds;
		bool m_stripVisible = false;
	};
}
