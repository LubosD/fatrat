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

#ifndef SETTINGSRSSFORM_H
#define SETTINGSRSSFORM_H
#include <QObject>

#include "RssFetcher.h"
#include "WidgetHostChild.h"
#include "ui_SettingsRssForm.h"

class SettingsRssForm : public QObject,
                        public WidgetHostChild,
                        Ui_SettingsRssForm {
  Q_OBJECT
 public:
  SettingsRssForm(QWidget* w, QObject* parent);
  virtual void load();
  virtual void accepted();
  static WidgetHostChild* create(QWidget* w, QObject* parent) {
    return new SettingsRssForm(w, parent);
  }

  static void applySettings();
 protected slots:
  void feedAdd();
  void feedEdit();
  void feedDelete();

  void regexpAdd();
  void regexpEdit();
  void regexpDelete();

 private:
  QList<RssFeed> m_feeds;
  QList<RssRegexp> m_regexps;
};

#endif
