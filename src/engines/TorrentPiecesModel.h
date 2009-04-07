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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#ifndef TORRENTPIECESMODEL_H
#define TORRENTPIECESMODEL_H
#include <QAbstractListModel>
#include <QItemDelegate>
#include <vector>
#include <QStringList>
#include <libtorrent/session.hpp>

class TorrentDownload;

class BlockDelegate : public QItemDelegate
{
Q_OBJECT
public:
	BlockDelegate(QObject* parent=0) : QItemDelegate(parent) {}
	QSize sizeHint(const QStyleOptionViewItem& /*option*/, const QModelIndex& /*index*/) const { return QSize(60,20); }
	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
};

class TorrentPiecesModel : public QAbstractListModel
{
Q_OBJECT
public:
	TorrentPiecesModel(QObject* parent, TorrentDownload* d);
	QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const;
	QModelIndex parent(const QModelIndex &index) const;
	int rowCount(const QModelIndex &parent = QModelIndex()) const;
	int columnCount(const QModelIndex&) const { return m_columns.size(); }
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
	QVariant data(const QModelIndex &index, int role) const;
	bool hasChildren ( const QModelIndex & parent = QModelIndex() ) const;
	
	void refresh();
private:
	TorrentDownload* m_download;
	int m_nLastRowCount;
	QStringList m_columns;
protected:
	std::vector<libtorrent::partial_piece_info> m_pieces;
	friend class BlockDelegate;
};

#endif
