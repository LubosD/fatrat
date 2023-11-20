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

#include "DropBox.h"

#include <QBitmap>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QSettings>
#include <QtDebug>

#include "MainWindow.h"
#include "Settings.h"

extern MainWindow* g_wndMain;
extern QSettings* g_settings;

DropBox::DropBox(QWidget* parent) : QWidget(parent) {
  setWindowFlags(Qt::ToolTip);
  setAcceptDrops(true);

  m_renderer = new QSvgRenderer(QString(":/svg/mousetrap.svg"), this);
  move(g_settings->value("dropbox/position", QPoint(100, 100)).toPoint());

  reconfigure();
}

void DropBox::reconfigure() {
  int myheight =
      g_settings->value("dropbox/height", getSettingsDefault("dropbox/height"))
          .toInt();
  resize(myheight, myheight);

  m_buffer = QPixmap(size());

  QPainter p(&m_buffer);
  p.setViewport(0, 0, width(), height());
  p.eraseRect(0, 0, width(), height());
  m_renderer->render(&p);

  setMask(m_buffer.createMaskFromColor(Qt::white));

  m_bUnhide =
      g_settings->value("dropbox/unhide", getSettingsDefault("dropbox/unhide"))
          .toBool();
}

void DropBox::paintEvent(QPaintEvent*) {
  QPainter pt(this);
  pt.drawPixmap(0, 0, m_buffer);
}

void DropBox::mousePressEvent(QMouseEvent* event) {
  if (event->buttons() == Qt::LeftButton) {
    m_mx = event->globalX();
    m_my = event->globalY();
  } else if (event->buttons() == Qt::MiddleButton)
    g_wndMain->trayIconActivated(QSystemTrayIcon::MiddleClick);
}

void DropBox::mouseMoveEvent(QMouseEvent* event) {
  if (event->buttons() == Qt::LeftButton) {
    move(x() + event->globalX() - m_mx, y() + event->globalY() - m_my);
    mousePressEvent(event);
  }
}

void DropBox::mouseReleaseEvent(QMouseEvent*) {
  g_settings->setValue("dropbox/position", pos());
}

void DropBox::dragEnterEvent(QDragEnterEvent* event) {
  g_wndMain->dragEnterEvent(event);
}

void DropBox::dropEvent(QDropEvent* event) {
  if (m_bUnhide) g_wndMain->showWindow(true);
  g_wndMain->dropEvent(event);
}
