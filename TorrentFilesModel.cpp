#include "TorrentDownload.h"
#include "TorrentFilesModel.h"
#include "TorrentProgressWidget.h"
#include "fatrat.h"
#include <QPainter>
#include <QtDebug>

TorrentFilesModel::TorrentFilesModel(QObject* parent, TorrentDownload* d)
	: QAbstractListModel(parent), m_download(d), m_pieces(0)
{
	m_columns << tr("File name") << tr("File size") << tr("Progress");
	m_columns << tr("Priority") << tr("Progress display");
}

QModelIndex TorrentFilesModel::index(int row, int column, const QModelIndex& parent) const
{
	if(!parent.isValid())
		return createIndex(row,column,(void*)this);
	else
		return QModelIndex();
}

QModelIndex TorrentFilesModel::parent(const QModelIndex &index) const
{
	return QModelIndex();
}

int TorrentFilesModel::rowCount(const QModelIndex &parent) const
{
	return m_files.size();
}

QVariant TorrentFilesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
		return m_columns[section];
	return QVariant();
}

QVariant TorrentFilesModel::data(const QModelIndex &index, int role) const
{
	int i = index.row();
	
	if(i >= m_files.size())
		return QVariant();
		
	if(role == Qt::DisplayRole)
	{
		switch(index.column())
		{
			case 0:
				return QString::fromUtf8(m_files[i].path.string().c_str());
			case 1:
				return formatSize(m_files[i].size);
			case 2:
				if(!m_progresses.empty())
					return QString("%1%").arg(m_progresses[i]*100, 0, 'f', 1);
				break;
			case 3:
				switch(m_priorities[i])
				{
					case 0:
						return tr("Do not download");
					case 1:
						return tr("Normal");
					case 4:
						return tr("Increased");
					case 7:
						return tr("Maximum");
				}
		}
	}
	return QVariant();
}

bool TorrentFilesModel::hasChildren(const QModelIndex& parent) const
{
	return !parent.isValid();
}

void TorrentFilesModel::fill()
{
	for(libtorrent::torrent_info::file_iterator it=m_download->m_info.begin_files();it!=m_download->m_info.end_files();it++)
	{
		m_files << *it;
	}
	
	for(int i=0;i<m_files.size();i++)
		m_priorities << 1;
}

void TorrentFilesModel::refresh(const std::vector<bool>* pieces)
{
	/*int piece_size = m_download->m_info.piece_length();
	for(int i=0;i<m_files.size();i++)
	{
		qint64 completed;
		int piece_start = m_files[i].offset / piece_size;
		int piece_end = (int) ((m_files[i].offset + m_files[i].size) / piece_size + 0.5f) - 1;
		int piece = piece_start + 1;
		
		if((*pieces)[piece_start])
			completed = piece_size - (m_files[i].offset % piece_size);
		else
			completed = 0;
		
		while(piece < piece_end)
		{
			if((*pieces)[piece])
				completed += piece_size;
			piece++;
		}
		
		if(piece_end > piece_start && (*pieces)[piece_end])
			completed += (m_files[i].offset + m_files[i].size) % piece_size;
		
		if(completed > m_files[i].size)
			completed = m_files[i].size;
		
		float newval = 100.0/m_files[i].size*completed;
		
		
		if(newval != m_progresses[i])
		{
			m_progresses[i] = newval;
			dataChanged(createIndex(i, 2), createIndex(i, m_columns.size())); // refresh the view
		}
	}*/
	m_pieces = pieces;
	
	m_download->m_handle.file_progress(m_progresses);
	dataChanged(createIndex(0, 2), createIndex(m_files.size(), m_columns.size())); // refresh the view
}

void TorrentProgressDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	TorrentFilesModel* model = (TorrentFilesModel*) index.internalPointer();
	
	if(index.column() == 4 && model->m_pieces)
	{
		int row = index.row();
		QRect myrect = option.rect;
		
		myrect.setWidth(myrect.width()-1);
		
		quint32* buf = new quint32[myrect.width()];
		
		const libtorrent::file_entry& file = model->m_files[row];
		int piece_size = model->m_download->m_info.piece_length();
		
		QImage im;
		float sstart,send;
		
		sstart = (file.offset % piece_size) / float(piece_size);
		send = 1.0f - ((file.offset + file.size) % piece_size) / float(piece_size);
		
		int start = file.offset/piece_size;
		int end = ceilf( (file.offset+file.size)/float(piece_size) );
		
		std::vector<bool> pieces = std::vector<bool>(model->m_pieces->begin() + start,
				model->m_pieces->begin() + end );
		
		im = TorrentProgressWidget::generate(pieces, myrect.width(), buf , sstart, send);
		
		painter->drawImage(option.rect, im);
		painter->setPen(Qt::black);
		painter->drawRect(myrect);
		
		delete [] buf;
	}
	else
		QItemDelegate::paint(painter, option, index);
}
