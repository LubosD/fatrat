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

#ifndef RSSREGEXPDLG_H
#define RSSREGEXPDLG_H
#include <QDialog>

#include "RssFetcher.h"
#include "ui_RssRegexpDlg.h"

class RssRegexpDlg : public QDialog, Ui_RssRegexpDlg {
  Q_OBJECT
 public:
  RssRegexpDlg(QWidget* parent);
  int exec();
 protected slots:
  void browse();
  void test();
  void updateTVS();
  void updateParsing();
  void linkClicked(const QString& link);
  void queueChanged(int now);

 public:
  QList<RssFeed> m_feeds;

  QString m_strFeedName;
  RssRegexp m_regexp;
  int m_nLastQueue;
};

#endif
