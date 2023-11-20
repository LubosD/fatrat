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

#ifndef TORRENTSEARCH_H
#define TORRENTSEARCH_H
#include <QList>
#include <QPair>
#include <QWidget>

#include "ui_TorrentSearch.h"

class QNetworkAccessManager;
class QNetworkReply;

class TorrentSearch : public QWidget, Ui_TorrentSearch {
  Q_OBJECT
 public:
  TorrentSearch();
  virtual ~TorrentSearch();
  static QWidget* create() { return new TorrentSearch; }

 protected:
  struct RegExpParam {
    QString regexp;
    int field, match;
  };

  struct Engine {
    QString id, name;
    QString query, postData;

    // parsing blocks
    QString beginning, splitter, ending;

    // parsing items
    QMap<QString, RegExpParam> regexps;

    QMap<QString, QString> formats;

    QNetworkReply* reply;

    bool operator<(const Engine& e) const { return this->name < e.name; }
  };

  void updateUi();
  void loadEngines();
  void loadEngines(QString path);
  void parseResults(Engine* e, const QByteArray& data);
  QString completeUrl(QString url, QString complete);

  static QList<QByteArray> splitArray(const QByteArray& src, QString sep);
 signals:
  void changeTabTitle(QString newTitle);
 public slots:
  void search();
  void searchDone(QNetworkReply* reply);
  void download();
  void setSearchFocus();
  void resultsContext(const QPoint&);
  void openDetails();

 private:
  QList<Engine> m_engines;
  QNetworkAccessManager* m_network;
  bool m_bSearching;
  int m_nActiveTotal, m_nActiveDone;
};

class SearchTreeWidgetItem : public QTreeWidgetItem {
 public:
  SearchTreeWidgetItem(QTreeWidget* parent) : QTreeWidgetItem(parent) {}

  void parseSize(QString in);

  QString m_strLink;  // torrent download link
  QString m_strInfo;  // info URL
  qint64 m_nSize;     // torrent's data size
 private:
  bool operator<(const QTreeWidgetItem& other) const;
};

#endif
