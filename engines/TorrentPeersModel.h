/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
USA.
*/

#ifndef TORRENTPEERSMODEL_H
#define TORRENTPEERSMODEL_H
#include <QAbstractListModel>
#include <QStringList>
#include <vector>
#include <libtorrent/session.hpp>

class TorrentDownload;

class TorrentPeersModel : public QAbstractListModel
{
Q_OBJECT
public:
	TorrentPeersModel(QObject* parent, TorrentDownload* d);
	
	QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const;
	QModelIndex parent(const QModelIndex &index) const;
	int rowCount(const QModelIndex &parent = QModelIndex()) const;
	int columnCount(const QModelIndex&) const { return m_columns.size(); }
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
	QVariant data(const QModelIndex &index, int role) const;
	bool hasChildren ( const QModelIndex & parent = QModelIndex() ) const;
	
	void refresh();
protected:
	std::vector<libtorrent::peer_info> m_peers;
private:
	TorrentDownload* m_download;
	int m_nLastRowCount;
	QStringList m_columns;
	
	friend class TorrentDetails;
};

#endif
