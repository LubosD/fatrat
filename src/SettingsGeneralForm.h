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

#ifndef _SETTINGSGENERALFORM_H
#define _SETTINGSGENERALFORM_H
#include "WidgetHostChild.h"
#include "ui_SettingsGeneralForm.h"

class SettingsGeneralForm : public QObject,
                            Ui_SettingsGeneralForm,
                            public WidgetHostChild {
  Q_OBJECT
 public:
  SettingsGeneralForm(QWidget* me, QObject* parent);
  virtual void load();
  virtual void accepted();
  static WidgetHostChild* create(QWidget* me, QObject* parent) {
    return new SettingsGeneralForm(me, parent);
  }

  static void applySettings();
 private slots:
  void manageFavs();
};

#endif
