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

#ifndef TRANSFERSVIEW_H
#define TRANSFERSVIEW_H
#include <QDrag>
#include <QHeaderView>
#include <QPainter>
#include <QPixmap>
#include <QTreeView>

class TransfersView : public QTreeView {
  Q_OBJECT
 public:
  TransfersView(QWidget *parent) : QTreeView(parent) {}

 protected:
  virtual void startDrag(Qt::DropActions supportedActions)  // from Qt
  {
    QModelIndexList indexes = selectedIndexes();
    if (indexes.count() > 0) {
      QMimeData *data = model()->mimeData(indexes);
      if (!data) return;

      QRect rect;
      QPixmap pixmap(100, 100);
      QPainter painter;
      QString text;

      pixmap.fill(Qt::transparent);
      painter.begin(&pixmap);

      text = tr("%1 transfers").arg(indexes.size() / header()->count());
      painter.drawText(rect, 0, text, &rect);

      rect.moveTo(5, 5);
      QRect around = rect.adjusted(-5, -5, 5, 5);
      painter.fillRect(around, Qt::white);

      painter.drawText(rect, 0, text, 0);

      painter.setPen(Qt::darkGray);
      painter.drawRect(around);

      painter.end();

      QDrag *drag = new QDrag(this);
      drag->setPixmap(pixmap);
      drag->setMimeData(data);
      drag->setHotSpot(QPoint(-20, -20));

      drag->exec(supportedActions);
    }
  }
};

#endif
