#pragma once
#include "Models.h"
#include <QSet>
#include <QStringList>
#include <QVector>

namespace tui
{
	// What the current selection allows, like the web UI's availableTransferActions()
	struct Availability
	{
		bool resume = false, force = false, pause = false, del = false, moveUp = false, moveDown = false, edit = false, any = false;
	};

	// The client side state of queues and the transfers of the selected queue. No I/O, no drawing.
	class Store
	{
	public:
		// ---- queues
		const QVector<QueueInfo>& queues() const { return m_queues; }
		int queueIndex() const { return m_queueIndex; }
		const QueueInfo* currentQueue() const;
		QString currentQueueUuid() const;
		int queueCount(const QString& uuid) const;
		void setQueueCount(const QString& uuid, int n) { m_counts[uuid] = n; }
		void setQueues(const QVector<QueueInfo>& q);
		void selectQueue(int index);
		void selectQueueByUuid(const QString& uuid);
		void moveQueueCursor(int delta);

		// ---- transfers of the selected queue
		const QVector<TransferInfo>& transfers() const { return m_transfers; }
		// Ignored if the data belongs to a queue that is no longer selected
		bool setTransfers(const QString& queue, const QVector<TransferInfo>& t);
		void clearTransfers();
		// indexes into transfers() that pass the filter
		QVector<int> visible() const;
		int cursor() const { return m_cursor; }              // index into visible()
		void setCursor(int visibleIndex);
		void moveCursor(int delta);
		void cursorHome();
		void cursorEnd();
		const TransferInfo* current() const;
		void setCursorToUuid(const QString& uuid);

		// ---- marks and targets
		const QSet<QString>& marks() const { return m_marks; }
		void toggleMark();
		void toggleMarkAt(int visibleIndex);
		void markRange(int fromVisible, int toVisible);
		void markAll();
		void clearMarks();
		// The marked transfers (in list order), or the current one if nothing is marked
		QStringList targets() const;
		QVector<const TransferInfo*> targetTransfers() const;
		Availability availability() const;

		// ---- filter
		const QString& filter() const { return m_filter; }
		void setFilter(const QString& f);

		// ---- stats
		const Stats& stats() const { return m_stats; }
		void setStats(const Stats& s) { m_stats = s; }
		SpeedPair queueSpeeds(const QString& uuid) const { return m_stats.queues.value(uuid); }

		// Updates the visible state at once; the next poll reconciles it
		void setStateLocally(const QStringList& uuids, TransferInfo::State s);

	private:
		void clampCursor();

		QVector<QueueInfo> m_queues;
		QMap<QString, int> m_counts;
		int m_queueIndex = 0;
		QVector<TransferInfo> m_transfers;
		int m_cursor = 0;
		QString m_cursorUuid;
		QSet<QString> m_marks;
		QString m_filter;
		Stats m_stats;
	};
}
