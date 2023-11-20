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

#ifndef HTTPDETAILS_H
#define HTTPDETAILS_H
#include <QColor>
#include <QMenu>
#include <QTimer>
#include <QWidget>

#include "ui_HttpDetails.h"

class CurlDownload;

class HttpDetails : public QObject, Ui_HttpDetails {
  Q_OBJECT
 public:
  HttpDetails(QWidget* w);
  void setDownload(CurlDownload* d);
 private slots:
  void addSegment();
  void deleteSegment();
  void addUrl();
  void editUrl();
  void deleteUrl();
  void refresh();
  void addSegmentUrl();
  void mirrorSearch();

 protected:
  static QMap<QString, QStringList> loadMirrors();

 private:
  QTimer m_timer;
  CurlDownload* m_download;
  QMenu m_menu;
  // contains only URL actions
  QList<QAction*> m_menuActions;
  QAction* m_separator;

  class GradientWidget : public QWidget {
   public:
    GradientWidget(QColor color);
    inline QColor color() const { return m_color; }
    inline void setColor(QColor c) {
      m_color = c;
      update();
    }

   protected:
    virtual void paintEvent(QPaintEvent* event);

   private:
    QColor m_color;
  };
};

#endif
