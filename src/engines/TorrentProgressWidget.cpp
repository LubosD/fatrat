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

#include "TorrentProgressWidget.h"

#include <QPainter>
#include <QtDebug>
#include <algorithm>

TorrentProgressWidget::TorrentProgressWidget(QWidget* parent)
    : QWidget(parent) {
  m_data = new quint32[1000];
}

TorrentProgressWidget::~TorrentProgressWidget() { delete[] m_data; }

void TorrentProgressWidget::generate(const libtorrent::bitfield& data) {
  m_image = generate(data, 1000, m_data);
  update();
}

void TorrentProgressWidget::generate(const std::vector<int>& data) {
  m_image = generate(data, 1000, m_data);
  update();
}

// blue colored
QImage TorrentProgressWidget::generate(const libtorrent::bitfield& data,
                                       int width, quint32* buf, float sstart,
                                       float send) {
  double fact = (data.size() - send - sstart) / float(width);
  double step = qMin<double>(255.0, 255.0 / fact);

  for (int i = 0; i < width; i++) {
    int from = i * fact + sstart;
    int to = (i + 1) * fact + sstart;

    if (to >= (int)data.size()) to = data.size() - 1;

    double color = 0;
    do {
      color += data[from] ? step : 0;
      from++;
    } while (from <= to);

    quint32 rcolor = 255 - qMin(quint32(color), 255U);
    buf[i] = 0xff0000ff | (rcolor << 8) | (rcolor << 16);
  }

  return QImage((uchar*)buf, width, 1, QImage::Format_RGB32);
}

// grey colored
QImage TorrentProgressWidget::generate(const std::vector<int>& data, int width,
                                       quint32* buf, float sstart, float send) {
  if (send < 0) send = data.size();

  const int maximum =
      (data.empty()) ? 0 : *std::max_element(data.begin(), data.end());
  const float step = (send - sstart) / width;

  if (maximum > 0) {
    for (int i = 0; i < width; i++) {
      int from = i * step + sstart;
      int to = (i + 1) * step + sstart;
      int total = 0;

      if (to >= (int)data.size()) to = data.size() - 1;

      for (int j = from; j <= to; j++) total += data[j];

      int rcolor =
          255 - qMin(255.0 * total / ((to - from + 1) * maximum), 255.0);
      buf[i] = 0xff000000 | (rcolor) | (rcolor << 8) | (rcolor << 16);
    }
  } else
    memset(buf, 0xff, 4 * width);

  return QImage((uchar*)buf, width, 1, QImage::Format_RGB32);
}

void TorrentProgressWidget::paintEvent(QPaintEvent* event) {
  QPainter painter;
  painter.begin(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setClipRegion(event->region());

  QImage bdraw = m_image.scaled(size());
  painter.drawImage(0, 0, bdraw);

  painter.end();
}
