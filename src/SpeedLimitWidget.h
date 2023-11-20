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

#ifndef SPEEDLIMITWIDGET_H
#define SPEEDLIMITWIDGET_H
#include <QLabel>
#include <QTimer>
#include <QWidget>

class RightClickLabel : public QLabel {
  Q_OBJECT
 public:
  RightClickLabel(QWidget* parent)
      : QLabel(parent), m_nSpeed(0), m_bUpload(false) {}
  void setUpload(bool is) { m_bUpload = is; }
  void refresh(int speed) { m_nSpeed = speed; }
 public slots:
  void setLimit();
  void customSpeedEntered();

 protected:
  void mousePressEvent(QMouseEvent* event);
  void mouseDoubleClickEvent(QMouseEvent* event);

  int m_nSpeed;
  bool m_bUpload;
};

#include "ui_SpeedLimitWidget.h"

class SpeedLimitWidget : public QWidget, Ui_SpeedLimitWidget {
  Q_OBJECT
 public:
  SpeedLimitWidget(QWidget* parent);
 public slots:
  void refresh();

 protected:
  void mouseDoubleClickEvent(QMouseEvent*);

 private:
  QTimer m_timer;
};

#endif
