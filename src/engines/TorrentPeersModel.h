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

#ifndef TORRENTPEERSMODEL_H
#define TORRENTPEERSMODEL_H
#include <QAbstractListModel>
#include <QStringList>
#include <libtorrent/peer_info.hpp>
#include <libtorrent/session.hpp>
#include <vector>

class TorrentDownload;

class TorrentPeersModel : public QAbstractListModel {
  Q_OBJECT
 public:
  TorrentPeersModel(QObject *parent, TorrentDownload *d);

  QModelIndex index(int row, int column = 0,
                    const QModelIndex &parent = QModelIndex()) const;
  QModelIndex parent(const QModelIndex &index) const;
  int rowCount(const QModelIndex &parent = QModelIndex()) const;
  int columnCount(const QModelIndex &) const { return m_columns.size(); }
  QVariant headerData(int section, Qt::Orientation orientation, int role) const;
  QVariant data(const QModelIndex &index, int role) const;
  bool hasChildren(const QModelIndex &parent = QModelIndex()) const;

  void refresh();

 protected:
  std::vector<libtorrent::peer_info> m_peers;

 private:
  TorrentDownload *m_download;
  int m_nLastRowCount;
  QStringList m_columns;

  friend class TorrentDetails;
};

#endif
