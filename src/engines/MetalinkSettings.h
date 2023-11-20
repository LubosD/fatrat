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

#ifndef METALINKSETTINGS_H
#define METALINKSETTINGS_H
#include <QWidget>

#include "WidgetHostChild.h"
#include "ui_SettingsMetalinkForm.h"

class MetalinkSettings : public QObject,
                         public WidgetHostChild,
                         Ui_SettingsMetalinkForm {
  Q_OBJECT
 public:
  MetalinkSettings(QWidget* w, QObject* p);
  static WidgetHostChild* create(QWidget* w, QObject* p) {
    return new MetalinkSettings(w, p);
  }
  virtual void load();
  virtual void accepted();
};

#endif  // METALINKSETTINGS_H
