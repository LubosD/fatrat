#include "QueueToolTip.h"
#include "fatrat.h"

QueueToolTip::QueueToolTip(QWidget* parent, Queue* queue)
	: BaseToolTip(queue, parent), m_queue(queue)
{
}

void QueueToolTip::refresh()
{
	if(!isVisible())
		return;
	
	QString text;
	const Queue::Stats& stats = m_queue->m_stats;
	
	if((QCursor::pos() - pos()).manhattanLength() > 60)
		hide();
	else
	{
		text = tr("<table cellspacing=4><tr><td><font color=green>Active:</font></td><td>%1 down</td><td>%2 up</td></tr>"
				"<tr><td><font color=red>Waiting:</font></td><td>%3 down</td><td>%4 up</td></tr>"
				"<tr><td><font color=blue>Speed:</font></td><td>%5 down</td><td>%6 up</td></tr></table>")
				.arg(stats.active_d).arg(stats.active_u).arg(stats.waiting_d).arg(stats.waiting_u)
				.arg(formatSize(stats.down, true)).arg(formatSize(stats.up, true));
		setText(text);
		resize(sizeHint());
	}
}

