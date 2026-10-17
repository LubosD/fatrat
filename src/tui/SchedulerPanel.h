#pragma once
#include "App.h"
#include <QVector>
#include <functional>
#include <memory>

namespace tui
{
	namespace scheduler
	{
		struct Entry
		{
			QString id, name, queue, action, time, at;
			bool repeated = true;
			QVector<bool> days;
			bool hasLimit = false;
			qint64 down = 0, up = 0;     // bytes per second, 0 = unlimited

			static Entry fromJson(const QJsonObject& o);
		};

		QString daysLabel(const QVector<bool>& days);          // "Every day", "Mon-Fri", "Mon, Wed, Sun"
		QString actionLabel(const Entry& a);
		QString whenLabel(const Entry& a);
	}

	// The Scheduler page of the settings window: the scheduled actions as a list that is edited in place. Unlike the
	// other pages nothing is staged; every change goes to the server at once.
	class SchedulerPanel
	{
	public:
		using AddClick = std::function<void(const Rect&, std::function<void()>)>;

		explicit SchedulerPanel(App& app);
		~SchedulerPanel();

		void load();
		void draw(Canvas& c, const Rect& area, bool active, const AddClick& addClick);
		// True if the event was used
		bool key(const Event& e);
		QVector<Hint> hints();

		int count() const { return m_actions.size(); }

	private:
		enum class State { Idle, Loading, Ready, Failed };

		void edit(const scheduler::Entry* existing);
		void remove();
		void select(int i);

		App& m_app;
		State m_state = State::Idle;
		QString m_error;
		QVector<scheduler::Entry> m_actions;
		int m_sel = 0;
		std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
	};
}
