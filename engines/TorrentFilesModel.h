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

#ifndef TORRENTFILESMODEL_H
#define TORRENTFILESMODEL_H
#include <QAbstractListModel>
#include <QList>
#include <QVector>
#include <QStringList>
#include <QItemDelegate>
#include <vector>
#include <libtorrent/session.hpp>

class TorrentDownload;

class TorrentProgressDelegate : public QItemDelegate
{
public:
	TorrentProgressDelegate(QObject* parent = 0) : QItemDelegate(parent) {}
	QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const { return QSize(60,20); }
	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
};

class TorrentFilesModel : public QAbstractListModel
{
Q_OBJECT
public:
	TorrentFilesModel(QObject* parent, TorrentDownload* d);
	
	QModelIndex index(int row, int column, const QModelIndex& parent) const;
	QModelIndex parent(const QModelIndex &index) const;
	int rowCount(const QModelIndex &parent) const;
	int columnCount(const QModelIndex&) const { return m_columns.size(); }
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
	QVariant data(const QModelIndex &index, int role) const;
	bool hasChildren(const QModelIndex& parent) const;
	
	void fill();
	void refresh(const std::vector<bool>* pieces);
protected:
	QList<libtorrent::file_entry> m_files;
	std::vector<float> m_progresses;
	const std::vector<bool>* m_pieces;
private:
	TorrentDownload* m_download;
	QStringList m_columns;
	
	friend class TorrentProgressDelegate;
};

#endif
