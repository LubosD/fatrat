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

#include "SpeedGraph.h"

#include <QFileDialog>
#include <QMenu>
#include <QtDebug>

#include "Queue.h"
#include "Settings.h"
#include "Transfer.h"
#include "fatrat.h"

SpeedGraph::SpeedGraph(QWidget* parent)
    : QWidget(parent), m_queue(0), m_transfer(0) {
  m_timer = new QTimer(this);
  connect(m_timer, SIGNAL(timeout()), this, SLOT(update()));
  m_timer->start(getSettingsValue("gui_refresh").toInt());
}

void SpeedGraph::setRenderSource(Transfer* t) {
  if (m_transfer == t) return;
  disconnect(this, SLOT(setNull()));

  m_transfer = t;
  if (m_transfer)
    connect(m_transfer, SIGNAL(destroyed()), this, SLOT(setNull()));
  update();
}

void SpeedGraph::setRenderSource(Queue* q) {
  if (m_queue == q) return;
  disconnect(this, SLOT(setNull()));

  m_queue = q;
  if (m_queue) connect(m_queue, SIGNAL(destroyed()), this, SLOT(setNull()));
  update();
}

void SpeedGraph::contextMenuEvent(QContextMenuEvent* event) {
  QMenu menu;
  QAction* act = menu.addAction(tr("Save as..."));

  connect(act, SIGNAL(triggered()), this, SLOT(saveScreenshot()));

  menu.exec(mapToGlobal(event->pos()));
}

void SpeedGraph::saveScreenshot() {
  QString file;
  QImage image(size(), QImage::Format_RGB32);

  if (m_transfer)
    draw(m_transfer->speedData(), size(), &image);
  else if (m_queue)
    draw(m_queue->speedData(), size(), &image);
  else
    return;

  file = QFileDialog::getSaveFileName(this, "FatRat", QString(), "*.png");

  if (!file.isEmpty()) {
    if (!file.endsWith(".png", Qt::CaseInsensitive)) file += ".png";
    image.save(file, "PNG");
  }
}

void SpeedGraph::draw(QQueue<QPair<int, int> > data, QSize size,
                      QPaintDevice* device, QPaintEvent* event) {
  int top = 0;
  QPainter painter(device);
  int seconds = getSettingsValue("graphminutes").toInt() * 60;
  bool bFilled = getSettingsValue("graph_style").toInt() == 0;

  painter.setRenderHint(QPainter::Antialiasing);

  if (event != 0) {
    painter.setClipRegion(event->region());
    painter.fillRect(event->rect(), QBrush(Qt::white));
  } else
    painter.fillRect(QRect(QPoint(0, 0), size), QBrush(Qt::white));

  if (!data.size()) {
    drawNoData(size, painter);
    return;
  }

  for (int i = 0; i < data.size(); i++) {
    top = qMax(top, qMax(data[i].first, data[i].second));
  }
  if (!top || data.size() < 2) {
    drawNoData(size, painter);
    return;
  }

  top = qMax(top / 10 * 11, 10 * 1024);

  const int height = size.height();
  const int width = size.width();
  const int elems = data.size();
  qreal perpt = width / (qreal(qMax(elems, seconds)) - 1);
  qreal pos = width;
  QVector<QLine> lines(elems);
  QVector<QPoint> filler(elems + 2);

  for (int i = 0; i < data.size(); i++)  // download speed
  {
    float y = height - height / qreal(top) * data[elems - i - 1].first;
    filler[i] = QPoint(pos, y);
    if (i > 0) lines[i - 1] = QLine(filler[i - 1], filler[i]);
    pos -= perpt;
  }
  filler[elems] = QPoint(filler[elems - 1].x(), height);
  filler[elems + 1] = QPoint(filler[0].x(), height);

  painter.setPen(Qt::darkBlue);

  if (bFilled) {
    QColor blueFill(Qt::darkBlue);
    blueFill.setAlpha(64);
    painter.setBrush(blueFill);
    painter.drawPolygon(filler.constData(), filler.size(), Qt::OddEvenFill);
  }

  lines[elems - 1] = QLine(2, 7, 12, 7);
  painter.drawLines(lines.constData(), lines.size());

  pos = width;
  for (int i = 0; i < elems; i++)  // upload speed
  {
    float y = height - height / qreal(top) * data[elems - i - 1].second;
    filler[i] = QPoint(pos, y);
    if (i > 0) lines[i - 1] = QLine(filler[i - 1], filler[i]);
    pos -= perpt;
  }
  filler[elems] = QPoint(filler[elems - 1].x(), height);
  filler[elems + 1] = QPoint(filler[0].x(), height);

  painter.setPen(Qt::darkRed);

  if (bFilled) {
    QColor redFill(Qt::darkRed);
    redFill.setAlpha(64);
    painter.setBrush(redFill);
    painter.drawPolygon(filler.constData(), filler.size(), Qt::OddEvenFill);
  }

  lines[elems - 1] = QLine(2, 19, 12, 19);
  painter.drawLines(lines.constData(), lines.size());

  painter.setPen(Qt::black);
  for (int i = 0; i < 4; i++) {
    int x = width - (i + 1) * (width / 4);
    painter.drawLine(x, height, x, height - 15);
    painter.drawText(x + 2, height - 2,
                     tr("%1 mins ago").arg((seconds / 4) * (i + 1) / 60.0));
  }

  painter.drawText(15, 12, tr("Download"));
  painter.drawText(15, 24, tr("Upload"));

  for (int i = 1; i < 10; i++) {
    int pos = int(float(height) / 10.f * i);

    painter.setPen(QPen(Qt::gray, 1.0, Qt::DashLine));
    painter.drawLine(0, pos, width, pos);

    painter.setPen(Qt::black);
    painter.drawText(0, pos - 10,
                     formatSize(qulonglong(top / 10.f * (10 - i)), true));
  }
}

void SpeedGraph::paintEvent(QPaintEvent* event) {
  if (m_transfer)
    draw(m_transfer->speedData(), size(), this, event);
  else if (m_queue)
    draw(m_queue->speedData(), size(), this, event);
}

void SpeedGraph::drawNoData(QSize size, QPainter& painter) {
  QFont font = painter.font();

  font.setPixelSize(20);

  painter.setFont(font);
  painter.setPen(Qt::gray);
  painter.drawText(QRect(QPoint(0, 0), size),
                   Qt::AlignHCenter | Qt::AlignVCenter, tr("NO DATA"));
}
