#pragma once
#include "App.h"
#include "ListNav.h"

namespace tui
{
	struct RssFeed { QString id, name, url; };
	struct RssRule
	{
		QString id, feedId, expression, queue, target, tvs, from, to, linkRegexp;
		bool addPaused = false, includeRepacks = false, includeTrailers = false, excludeManuals = false;
	};

	class RssView : public View
	{
	public:
		explicit RssView(App& app) : View(app) {}
		Context context() const override { return m_rules ? Context::RssRules : Context::RssFeeds; }
		void draw(Canvas& c, HitMap& hits, const Rect& area) override;
		void perform(Action a) override;
		void mouse(const Event& e, const Hit* hit) override;
		void poll() override;
		void activated() override { m_dirty = true; poll(); }
		QString statusText() const override;

		void setData(const QVector<RssFeed>& f, const QVector<RssRule>& r) { m_feeds = f; m_ruleList = r; }

	private:
		QVector<const RssRule*> visibleRules() const;
		const RssFeed* currentFeed() const;
		const RssRule* currentRule() const;
		void feedForm(const RssFeed* existing);
		void ruleForm(const RssRule* existing);
		void testForm();
		void downloadedForm();
		void deleteCurrent();
		void menu(int x, int y);

		QVector<RssFeed> m_feeds;
		QVector<RssRule> m_ruleList;
		ListNav m_feedNav, m_ruleNav;
		bool m_rules = false, m_dirty = true;
		int m_ticks = 0;
		Rect m_feedBox, m_ruleBox;
		int m_rows = 10;
		bool m_loaded = false;
	};
}
