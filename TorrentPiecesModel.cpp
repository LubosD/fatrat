#include "TorrentPiecesModel.h"
#include "TorrentDownload.h"
#include <QPainter>

TorrentPiecesModel::TorrentPiecesModel(QObject* parent, TorrentDownload* d)
: QAbstractListModel(parent), m_download(d), m_nLastRowCount(0)
{
}

TorrentPiecesModel::~TorrentPiecesModel()
{
}

QModelIndex TorrentPiecesModel::index(int row, int column, const QModelIndex &parent) const
{
	if(!parent.isValid())
		return createIndex(row,column,(void*)this);
	else
		return QModelIndex();
}

QModelIndex TorrentPiecesModel::parent(const QModelIndex &index) const
{
	return QModelIndex();
}

int TorrentPiecesModel::rowCount(const QModelIndex &parent) const
{
	return m_pieces.size();
}

QVariant TorrentPiecesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch(section)
		{
		case 0:
			return tr("Piece ID");
		case 1:
			return tr("Block count");
		case 2:
			return tr("Completed blocks");
		case 3:
			return tr("Requested blocks");
		case 4:
			return tr("Block view");
		}
	}
	return QVariant();
}

QVariant TorrentPiecesModel::data(const QModelIndex &index, int role) const
{
	if(role == Qt::DisplayRole)
	{
		const libtorrent::partial_piece_info& info = m_pieces[index.row()];
		
		switch(index.column())
		{
		case 0:
			return info.piece_index;
		case 1:
			return info.blocks_in_piece;
		case 2:
			return (int) info.finished_blocks.count();
		case 3:
			return (int) info.requested_blocks.count();
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
		m_download->m_handle.get_download_queue(m_pieces);
		count = m_pieces.size();
	}
	
	if(count > m_nLastRowCount)
	{
		beginInsertRows(QModelIndex(), m_nLastRowCount, count);
		endInsertRows();
	}
	else if(count < m_nLastRowCount)
	{
		beginRemoveRows(QModelIndex(), count, m_nLastRowCount);
		endRemoveRows();
	}
	m_nLastRowCount = count;
	
	dataChanged(createIndex(0,0), createIndex(count,5)); // refresh the view
}

void BlockDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	if(index.column() == 4) // block view
	{
		TorrentPiecesModel* model = (TorrentPiecesModel*) index.internalPointer();
		const libtorrent::partial_piece_info& piece = model->m_pieces[index.row()];
		
		int bwidth = option.rect.width() / piece.blocks_in_piece;
		for(int i=0;i<piece.blocks_in_piece;i++)
		{
			if(piece.finished_blocks.test(i))
				painter->fillRect(option.rect.x()+i*bwidth, option.rect.y(), bwidth, option.rect.height(), QColor(128,128,255));
			else if(piece.requested_blocks.test(i))
				painter->fillRect(option.rect.x()+i*bwidth, option.rect.y(), bwidth, option.rect.height(), Qt::gray);
		}
		
		painter->setPen(Qt::black);
		painter->drawRect(option.rect);
	}
	else
		QItemDelegate::paint(painter, option, index);
}


