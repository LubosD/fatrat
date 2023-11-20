/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2010 Lubos Dolezel <lubos a dolezel.info>

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

#ifndef _SPEEDGRAPH_H
#define _SPEEDGRAPH_H
#include <QPaintEvent>
#include <QPainter>
#include <QQueue>
#include <QTimer>
#include <QWidget>

class Transfer;
class Queue;

class SpeedGraph : public QWidget {
  Q_OBJECT
 public:
  SpeedGraph(QWidget* parent);
  void setRenderSource(Transfer* t);
  void setRenderSource(Queue* q);
  static void draw(QQueue<QPair<int, int> > data, QSize size,
                   QPaintDevice* device, QPaintEvent* event = 0);
 public slots:
  void setNull() { setRenderSource((Queue*)NULL); }
  void saveScreenshot();

 protected:
  virtual void paintEvent(QPaintEvent* event);
  virtual void contextMenuEvent(QContextMenuEvent* event);
  static void drawNoData(QSize size, QPainter& painter);

  Queue* m_queue;
  Transfer* m_transfer;
  QTimer* m_timer;
};

#endif
