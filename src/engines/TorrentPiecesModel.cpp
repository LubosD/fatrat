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

#include "TorrentPiecesModel.h"
#include "TorrentDownload.h"
#include <QPainter>

TorrentPiecesModel::TorrentPiecesModel(QObject* parent, TorrentDownload* d)
: QAbstractListModel(parent), m_download(d), m_nLastRowCount(0)
{
	m_columns << tr("Piece ID") << tr("Block count");
	m_columns << tr("Completed blocks") << tr("Requested blocks") << tr("Blocks being written") << tr("Block view");
}

QModelIndex TorrentPiecesModel::index(int row, int column, const QModelIndex &parent) const
{
	if(!parent.isValid())
		return createIndex(row,column,(void*)this);
	else
		return QModelIndex();
}

QModelIndex TorrentPiecesModel::parent(const QModelIndex&) const
{
	return QModelIndex();
}

int TorrentPiecesModel::rowCount(const QModelIndex&) const
{
	return m_pieces.size();
}

QVariant TorrentPiecesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		return m_columns[section];
	}
	return QVariant();
}

QVariant TorrentPiecesModel::data(const QModelIndex &index, int role) const
{
	if(role == Qt::DisplayRole)
	{
		if(index.row() >= (int) m_pieces.size())
			return QVariant();
		const libtorrent::partial_piece_info& info = m_pieces[index.row()];
		
		switch(index.column())
		{
		case 0:
			return int(info.piece_index);
		case 1:
			return info.blocks_in_piece;
		case 2:
			return (int) info.finished;
		case 3:
			return (int) info.requested;
		case 4:
			return (int) info.writing;
		}
	}
	else if(role == Qt::SizeHintRole)
	{
		return QSize(50, 20);
	}
	return QVariant();
}

bool TorrentPiecesModel::hasChildren(const QModelIndex & parent) const
{
	return !parent.isValid();
}

void TorrentPiecesModel::refresh()
{
	int count = 0;
	
	if(m_download->m_handle.is_valid())
	{
		m_pieces.clear();
		m_download->m_handle.get_download_queue(m_pieces);
		count = m_pieces.size();
	}
	
	if(count > m_nLastRowCount)
	{
		beginInsertRows(QModelIndex(), m_nLastRowCount, count-1);
		endInsertRows();
	}
	else if(count < m_nLastRowCount)
	{
		beginRemoveRows(QModelIndex(), 0, m_nLastRowCount-count-1);
		endRemoveRows();
	}
	m_nLastRowCount = count;
	
	dataChanged(createIndex(0,0), createIndex(count, m_columns.size())); // refresh the view
}

void BlockDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	if(index.column() == 5) // block view
	{
		TorrentPiecesModel* model = (TorrentPiecesModel*) index.internalPointer();
		const libtorrent::partial_piece_info& piece = model->m_pieces[index.row()];
		
		QRect myrect = option.rect;
		
		myrect.setWidth(myrect.width()-1);
		//myrect.setHeight(myrect.height()-1);
		
		float bwidth = myrect.width() / float(piece.blocks_in_piece);
		for(int i=0;i<piece.blocks_in_piece;)
		{
			int from = i, to;
			const int state = piece.blocks[i].state;
			
			do
				to = i++;
			while(i<piece.blocks_in_piece && state == piece.blocks[i].state);
			
			if(state == libtorrent::block_info::finished)
				painter->fillRect(myrect.x()+from*bwidth, myrect.y(), ceilf(bwidth*(to-from+1)), myrect.height(), QColor(128,128,255));
			else if(state == libtorrent::block_info::requested)
				painter->fillRect(myrect.x()+from*bwidth, myrect.y(), ceilf(bwidth*(to-from+1)), myrect.height(), Qt::gray);
		}
		
		painter->setPen(Qt::black);
		painter->drawRect(myrect);
	}
	else
		QItemDelegate::paint(painter, option, index);
}


