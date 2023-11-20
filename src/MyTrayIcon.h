/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2012 Lubos Dolezel <lubos a dolezel.info>

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

#ifndef MYTRAYICON_H
#define MYTRAYICON_H
#include <QtDebug>
#include <cstdlib>

#include "tooltips/TrayToolTip.h"

class MyTrayIcon : public QSystemTrayIcon {
 public:
  MyTrayIcon(QWidget* parent) : QSystemTrayIcon(parent), m_tip(0) {
    const char* desktop = getenv("XDG_CURRENT_DESKTOP");

    if (!desktop || strcmp(desktop, "Unity") != 0) m_tip = new TrayToolTip;
  }

  ~MyTrayIcon() { delete m_tip; }

  bool event(QEvent* e) {
    if (m_tip && e->type() == QEvent::ToolTip) {
      m_tip->regMove();
      return true;
    } else
      return QSystemTrayIcon::event(e);
  }

 private:
  TrayToolTip* m_tip;
};

#endif
