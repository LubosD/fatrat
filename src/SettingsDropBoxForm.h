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

#ifndef SETTINGSDROPBOXFORM_H
#define SETTINGSDROPBOXFORM_H

#include <QSettings>

#include "Settings.h"
#include "WidgetHostChild.h"
#include "fatrat.h"
#include "ui_SettingsDropBoxForm.h"

extern QSettings* g_settings;

class SettingsDropBoxForm : public QObject,
                            Ui_SettingsDropBoxForm,
                            public WidgetHostChild {
  Q_OBJECT
 public:
  SettingsDropBoxForm(QWidget* me, QObject* parent) : QObject(parent) {
    setupUi(me);
  }

  virtual void load() {
    checkUnhide->setChecked(getSettingsValue("dropbox/unhide").toBool());
    spinHeight->setValue(getSettingsValue("dropbox/height").toInt());
  }

  virtual void accepted() {
    g_settings->setValue("dropbox/unhide", checkUnhide->isChecked());
    g_settings->setValue("dropbox/height", spinHeight->value());
  }
  static WidgetHostChild* create(QWidget* me, QObject* parent) {
    return new SettingsDropBoxForm(me, parent);
  }
};

#endif
