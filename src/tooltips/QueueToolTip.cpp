/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 3 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#include "QueueToolTip.h"

#include <cstdlib>

#include "fatrat.h"

QueueToolTip::QueueToolTip(QWidget* parent, Queue* queue)
    : BaseToolTip(queue, parent), m_queue(queue) {}

void QueueToolTip::fill() {
  QString text;
  const Queue::Stats& stats = m_queue->m_stats;
  QPoint diff = QCursor::pos() - pos() + QPoint(25, 25);

  if (abs(diff.x()) > 5 || abs(diff.y()) > 5)
    hide();
  else {
    text =
        tr("<table cellspacing=4><tr><td><font "
           "color=green>Active:</font></td><td>%1 down</td><td>%2 up</td></tr>"
           "<tr><td><font color=red>Waiting:</font></td><td>%3 down</td><td>%4 "
           "up</td></tr>"
           "<tr><td><font color=blue>Speed:</font></td><td>%5 down</td><td>%6 "
           "up</td></tr></table>")
            .arg(stats.active_d)
            .arg(stats.active_u)
            .arg(stats.waiting_d)
            .arg(stats.waiting_u)
            .arg(formatSize(stats.down, true))
            .arg(formatSize(stats.up, true));
    setText(text);
    resize(sizeHint());
  }
}

void QueueToolTip::refresh() {
  if (isVisible()) fill();
}
