/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation
with the OpenSSL special exemption.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "QueueToolTip.h"
#include "fatrat.h"
#include <cstdlib>

QueueToolTip::QueueToolTip(QWidget* parent, Queue* queue)
	: BaseToolTip(queue, parent), m_queue(queue)
{
}

void QueueToolTip::fill()
{
	QString text;
	const Queue::Stats& stats = m_queue->m_stats;
	QPoint diff = QCursor::pos() - pos() + QPoint(25, 25);
	
	if(abs(diff.x()) > 5 || abs(diff.y()) > 5)
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

void QueueToolTip::refresh()
{
	if(isVisible())
		fill();
}

