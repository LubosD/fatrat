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

#include "TorrentDownload.h"
#include "TorrentFilesModel.h"
#include "TorrentProgressWidget.h"
#include "fatrat.h"
#include <QPainter>
#include <QtDebug>
#include <libtorrent/torrent_info.hpp>

TorrentFilesModel::TorrentFilesModel(QObject* parent, TorrentDownload* d)
	: QAbstractListModel(parent), m_pieces(0), m_download(d)
{
	m_columns << tr("File name") << tr("Size") << tr("Progress");
	m_columns << tr("Priority") << tr("Progress display");
}

QModelIndex TorrentFilesModel::index(int row, int column, const QModelIndex& parent) const
{
	if(!parent.isValid())
		return createIndex(row,column,(void*)this);
	else
		return QModelIndex();
}

QModelIndex TorrentFilesModel::parent(const QModelIndex&) const
{
	return QModelIndex();
}

int TorrentFilesModel::rowCount(const QModelIndex&) const
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
			{
				QString name = QString::fromStdString(m_download->m_info->files().file_path(i));
				int p = name.indexOf('/');
				
				if(p != -1)
					name = QLatin1String("[...]") + name.mid(p);
				
				return name;
			}
			case 1:
				return formatSize(m_download->m_info->files().file_size(i));
			case 2:
				if(!m_progresses.empty())
				{
					int v = int( double(m_progresses[i])/double(m_download->m_info->files().file_size(i))*1000 );
					return QString("%1%").arg(v / 10.0, 0, 'f', 1);
				}
				break;
			case 3:
				switch(m_download->m_vecPriorities[i])
				{
					case 0:
						return tr("Do not download");
					case 1:
						return tr("Normal");
					case 2 ... 4:
						return tr("Increased");
					case 5 ... 7:
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

void TorrentFilesModel::refresh(const libtorrent::bitfield* pieces)
{
	m_pieces = pieces;
	
	if(m_download->m_handle.is_valid())
		m_download->m_handle.file_progress(m_progresses);
	dataChanged(createIndex(0, 2), createIndex(m_files.size(), m_columns.size())); // refresh the view
}

void TorrentProgressDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	TorrentFilesModel* model = (TorrentFilesModel*) index.internalPointer();
	
	if(index.column() == 4 && model->m_pieces && model->m_download)
	{
		int row = index.row();
		QRect myrect = option.rect;
		
		myrect.setWidth(myrect.width()-1);
		
		quint32* buf = new quint32[myrect.width()];
		
		const auto offset = model->m_download->m_info->files().file_offset(row);
		const auto size = model->m_download->m_info->files().file_size(row);
		int piece_size = model->m_download->m_info->piece_length();
		
		QImage im;
		float sstart,send;
		
		sstart = (offset % piece_size) / float(piece_size);
		send = 1.0f - ((offset + size) % piece_size) / float(piece_size);
		
		int start = offset/piece_size;
		int end = ceilf( (offset+size)/float(piece_size) );
		
		const int allocated = (end-start+7)/8;
		char* cpy = new char[allocated];
		const char* src = model->m_pieces->data();
		const int shift = start % 8;
		
		if(shift)
		{
			for(int i = 0; i < allocated; i++)
			{
				int x = start / 8 + i;
				char b = (src[x] << shift) | (src[x+1] >> (8-shift));
				cpy[i] = b;
			}
		}
		else
		{
			memcpy(cpy, src + start/8, allocated);
		}
		
		im = TorrentProgressWidget::generate(libtorrent::bitfield(cpy, end-start), myrect.width(), buf , sstart, send);
		
		delete [] cpy;
		
		painter->drawImage(option.rect, im);
		painter->setPen(Qt::black);
		painter->drawRect(myrect);
		
		delete [] buf;
	}
	else
		QItemDelegate::paint(painter, option, index);
}
