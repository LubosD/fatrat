#include "Store.h"
#include <algorithm>

namespace tui
{

const QueueInfo* Store::currentQueue() const
{
	return m_queueIndex >= 0 && m_queueIndex < m_queues.size() ? &m_queues[m_queueIndex] : nullptr;
}

QString Store::currentQueueUuid() const
{
	const QueueInfo* q = currentQueue();
	return q ? q->uuid : QString();
}

int Store::queueCount(const QString& uuid) const
{
	return m_counts.value(uuid, -1);
}

void Store::setQueues(const QVector<QueueInfo>& q)
{
	const QString selected = currentQueueUuid();
	m_queues = q;
	int idx = -1;
	for (int i = 0; i < q.size(); i++)
		if (q[i].uuid == selected)
			idx = i;
	if (idx < 0)
	{
		// the selected queue is gone: stay at the same position
		idx = qBound(0, m_queueIndex, qMax(0, int(q.size()) - 1));
		if (selected != (idx < q.size() ? q[idx].uuid : QString()))
			clearTransfers();
	}
	m_queueIndex = idx;
}

void Store::selectQueue(int index)
{
	if (m_queues.isEmpty())
		return;
	index = qBound(0, index, int(m_queues.size()) - 1);
	if (index == m_queueIndex)
		return;
	m_queueIndex = index;
	clearTransfers();
}

void Store::selectQueueByUuid(const QString& uuid)
{
	for (int i = 0; i < m_queues.size(); i++)
		if (m_queues[i].uuid == uuid)
			selectQueue(i);
}

void Store::moveQueueCursor(int delta)
{
	selectQueue(m_queueIndex + delta);
}

bool Store::setTransfers(const QString& queue, const QVector<TransferInfo>& t)
{
	if (queue != currentQueueUuid())
		return false;

	m_transfers = t;
	m_counts[queue] = t.size();

	QSet<QString> present;
	for (const TransferInfo& x : t)
		present.insert(x.uuid);
	m_marks.intersect(present);

	// Keep the cursor on the same transfer if it is still there
	const QVector<int> vis = visible();
	int pos = -1;
	for (int i = 0; i < vis.size(); i++)
		if (m_transfers[vis[i]].uuid == m_cursorUuid)
			pos = i;
	if (pos >= 0)
		m_cursor = pos;
	clampCursor();
	return true;
}

void Store::clearTransfers()
{
	m_transfers.clear();
	m_marks.clear();
	m_cursor = 0;
	m_cursorUuid.clear();
}

QVector<int> Store::visible() const
{
	QVector<int> v;
	for (int i = 0; i < m_transfers.size(); i++)
		if (m_filter.isEmpty() || m_transfers[i].name.contains(m_filter, Qt::CaseInsensitive))
			v.push_back(i);
	return v;
}

void Store::clampCursor()
{
	const QVector<int> vis = visible();
	m_cursor = vis.isEmpty() ? 0 : qBound(0, m_cursor, int(vis.size()) - 1);
	m_cursorUuid = vis.isEmpty() ? QString() : m_transfers[vis[m_cursor]].uuid;
}

void Store::setCursor(int visibleIndex)
{
	m_cursor = visibleIndex;
	clampCursor();
}

void Store::moveCursor(int delta) { setCursor(m_cursor + delta); }
void Store::cursorHome() { setCursor(0); }
void Store::cursorEnd() { setCursor(1 << 30); }

const TransferInfo* Store::current() const
{
	const QVector<int> vis = visible();
	return m_cursor >= 0 && m_cursor < vis.size() ? &m_transfers[vis[m_cursor]] : nullptr;
}

void Store::setCursorToUuid(const QString& uuid)
{
	const QVector<int> vis = visible();
	for (int i = 0; i < vis.size(); i++)
		if (m_transfers[vis[i]].uuid == uuid)
		{
			setCursor(i);
			return;
		}
}

void Store::toggleMark()
{
	const TransferInfo* t = current();
	if (!t)
		return;
	if (!m_marks.remove(t->uuid))
		m_marks.insert(t->uuid);
	moveCursor(1);
}

void Store::toggleMarkAt(int visibleIndex)
{
	const QVector<int> vis = visible();
	if (visibleIndex < 0 || visibleIndex >= vis.size())
		return;
	const QString& u = m_transfers[vis[visibleIndex]].uuid;
	if (!m_marks.remove(u))
		m_marks.insert(u);
}

void Store::markRange(int a, int b)
{
	const QVector<int> vis = visible();
	if (a > b)
		std::swap(a, b);
	for (int i = qMax(0, a); i <= b && i < vis.size(); i++)
		m_marks.insert(m_transfers[vis[i]].uuid);
}

void Store::markAll()
{
	const QVector<int> vis = visible();
	// a second press unmarks everything
	if (m_marks.size() == vis.size() && !vis.isEmpty())
	{
		m_marks.clear();
		return;
	}
	for (int i : vis)
		m_marks.insert(m_transfers[i].uuid);
}

void Store::clearMarks() { m_marks.clear(); }

QStringList Store::targets() const
{
	QStringList out;
	for (const TransferInfo& t : m_transfers)
		if (m_marks.contains(t.uuid))
			out << t.uuid;
	if (out.isEmpty())
		if (const TransferInfo* c = current())
			out << c->uuid;
	return out;
}

QVector<const TransferInfo*> Store::targetTransfers() const
{
	QVector<const TransferInfo*> out;
	const QStringList t = targets();
	for (const TransferInfo& x : m_transfers)
		if (t.contains(x.uuid))
			out.push_back(&x);
	return out;
}

Availability Store::availability() const
{
	Availability a;
	const QStringList t = targets();
	a.any = !t.isEmpty();
	a.force = a.del = a.any;
	a.edit = t.size() == 1;
	for (int i = 0; i < m_transfers.size(); i++)
	{
		const TransferInfo& x = m_transfers[i];
		if (!t.contains(x.uuid))
			continue;
		a.resume |= x.state != TransferInfo::Waiting && x.state != TransferInfo::Active;
		a.pause |= x.state != TransferInfo::Paused;
		a.moveUp |= i > 0;
		a.moveDown |= i < m_transfers.size() - 1;
	}
	return a;
}

void Store::setFilter(const QString& f)
{
	m_filter = f;
	clampCursor();
}

void Store::setStateLocally(const QStringList& uuids, TransferInfo::State s)
{
	for (TransferInfo& t : m_transfers)
		if (uuids.contains(t.uuid))
			t.state = s;
}

}
