/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2010 Lubos Dolezel <lubos a dolezel.info>

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

#include <QApplication>
#include <QPainter>
#include <QMimeData>
#include <QUrl>
#include <QtDebug>

#include "TransfersModel.h"
#include "fatrat.h"
#include "MainWindow.h"

extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;
extern MainWindow* g_wndMain;

using namespace std;

TransfersModel::TransfersModel(QObject* parent)
	: QAbstractListModel(parent), m_queue(-1), m_nLastRowCount(0)
{
	m_states[0] = new QIcon(":/states/waiting.png");
	m_states[1] = new QIcon(":/states/active.png");
	m_states[2] = new QIcon(":/states/forced_active.png");
	m_states[3] = new QIcon(":/states/paused.png");
	m_states[4] = new QIcon(":/states/failed.png");
	m_states[5] = new QIcon(":/states/completed.png");
	
	m_states[6] = new QIcon(":/states/waiting_upload.png");
	m_states[7] = new QIcon(":/states/distribute.png");
	m_states[8] = new QIcon(":/states/forced_distribute.png");
	m_states[9] = new QIcon(":/states/paused_upload.png");
	m_states[10] = new QIcon(":/states/failed.png");
	m_states[11] = new QIcon(":/states/completed_upload.png");
}

TransfersModel::~TransfersModel()
{
	qDeleteAll(m_states,m_states+12);
}

QModelIndex TransfersModel::index(int row, int column, const QModelIndex &parent) const
{
	if(!parent.isValid())
		return createIndex(row,column,(void*)this);
	else
		return QModelIndex();
}

QVariant TransfersModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch(section)
		{
		//case 0:
		//	return tr("State");
		case 0:
			return tr("Name");
		case 1:
			return tr("Progress");
		case 2:
			return tr("Size");
		case 3:
		case 4:
			return tr("Speed");
		case 5:
			return tr("Time left");
		case 6:
			return tr("Message");
		}
	}
	if(role == Qt::DecorationRole)
	{
		if(section == 3)
			return QIcon(":/states/download.png");
		else if(section == 4)
			return QIcon(":/states/upload.png");
	}

	return QVariant();
}

int TransfersModel::rowCount(const QModelIndex &parent) const
{
	int count = 0;
	if(!parent.isValid())
	{
		g_queuesLock.lockForRead();
		
		if(m_queue < g_queues.size() && m_queue >= 0)
			count = g_queues[m_queue]->size();
		
		g_queuesLock.unlock();
	}
	
	return count;
}

TransfersModel::RowData TransfersModel::createDataSet(Transfer* t)
{
	RowData newData;
	
	const qint64 total = t->total();
	
	newData.state = t->state();
	newData.name = t->name();
	newData.fProgress = (total) ? 100.0/t->total()*t->done() : 0;
	newData.progress = (total) ? QString("%1%").arg(newData.fProgress, 0, 'f', 1) : QString();
	newData.size = (total) ? formatSize(t->total()) : QString("?");
	
	if(t->isActive())
	{
		int down,up;
		t->speeds(down,up);
		
		if(down || t->mode() == Transfer::Download)
			newData.speedDown = formatSize(down, true);
		if(up || t->mode() == Transfer::Upload)
			newData.speedUp = formatSize(up, true);
		
		if(t->total())
		{
			QString s;
			int down,up;
			qulonglong totransfer = t->total() - t->done();
			
			t->speeds(down,up);
			
			if(t->primaryMode() == Transfer::Download)
			{
				if(down)
					newData.timeLeft = formatTime(totransfer/down);
			}
			else if(up)
				newData.timeLeft = formatTime(totransfer/up);
		}
	}
	
	newData.message = t->message();
	newData.mode = t->mode();
	newData.primaryMode = t->primaryMode();
	
	return newData;
}

void TransfersModel::refresh()
{
	int count = 0;
	Queue* q = 0;
	g_queuesLock.lockForRead();
	
	if(m_queue < g_queues.size() && m_queue >= 0)
	{
		q = g_queues[m_queue];
		count = q->size();
	}
	
	m_lastData.resize(count);
	
	QList<bool> changes;
	MainWindow* w = static_cast<MainWindow*>(getMainWindow());
	QString filter;

	if(w)
		filter = w->getFilterText();
	
	if(q != 0)
	{
		q->lock();
		
		int filtered = 0;
		for(int i=0,j=0;i<count;i++,j++)
		{
			Transfer* t = q->at(i);
			RowData newData;

			if (!filter.isEmpty() && !t->name().contains(filter, Qt::CaseInsensitive))
			{
				filtered++;
				j--;
				continue;
			}
			
			if(t != 0)
				newData = createDataSet(t);
			
			changes << (newData != m_lastData[j]);
			m_lastData[j] = newData;
		}
		count -= filtered;
		q->unlock();
	}
	g_queuesLock.unlock();
	
	if(count > m_nLastRowCount)
	{
		qDebug() << "Adding" << count - m_nLastRowCount << "rows";
		beginInsertRows(QModelIndex(), m_nLastRowCount, count-1);
		endInsertRows();
	}
	else if(count < m_nLastRowCount)
	{
		qDebug() << "Removing" << m_nLastRowCount - count << "rows";
		beginRemoveRows(QModelIndex(), count, m_nLastRowCount-1);
		endRemoveRows();
	}
	m_nLastRowCount = count;
	
	for(int i=0;i<count;)
	{
		if(!changes[i])
			i++;
		else
		{
			int from = i, to;
			while(i < count && changes[i])
				to = i++;
			
			dataChanged(createIndex(from,0), createIndex(to,5)); // refresh the view
		}
	}
}

QModelIndex TransfersModel::parent(const QModelIndex&) const
{
	return QModelIndex();
}

bool TransfersModel::hasChildren (const QModelIndex & parent) const
{
	return !parent.isValid();
}

int TransfersModel::columnCount(const QModelIndex&) const
{
	return 7;
}

QVariant TransfersModel::data(const QModelIndex &index, int role) const
{
	int row = index.row();
	
	if(role == Qt::DisplayRole)
	{
		if(row < m_lastData.size())
		{
			switch(index.column())
			{
			case 0:
				return m_lastData[row].name;
			case 1:
				return m_lastData[row].progress;
			case 2:
				return m_lastData[row].size;
			case 3:
				return m_lastData[row].speedDown;
			case 4:
				return m_lastData[row].speedUp;
			case 5:
				return m_lastData[row].timeLeft;
			case 6:
				return m_lastData[row].message;
			}
		}
	}
	else if(role == Qt::DecorationRole)
	{
		if(index.column() == 0 && row < m_lastData.size())
		{
			Transfer::State state = m_lastData[row].state;
			if(m_lastData[row].mode == Transfer::Upload)
			{
				if(state == Transfer::Completed && m_lastData[row].primaryMode == Transfer::Download)
					return *m_states[5]; // an exception for download-oriented transfers
				else
					return *m_states[state+6];
			}
			else
				return *m_states[state];
		}
	}
	/*else if(role == Qt::SizeHintRole)
	{
		return QSize(50, 16);
	}*/

	return QVariant();
}

void TransfersModel::setQueue(int q)
{
	m_queue = q;
	refresh();
}

Qt::DropActions TransfersModel::supportedDragActions() const
{
	return Qt::MoveAction;
}

Qt::ItemFlags TransfersModel::flags(const QModelIndex &index) const
{
	Qt::ItemFlags defaultFlags = QAbstractListModel::flags(index);

	if(index.isValid())
		return Qt::ItemIsDragEnabled | defaultFlags;
	else
		return defaultFlags;
}

QMimeData* TransfersModel::mimeData(const QModelIndexList&) const
{
	QReadLocker l(&g_queuesLock);
	QMimeData *mimeData = new QMimeData;
	QByteArray encodedData;
	QByteArray files;
	QList<int> sel = g_wndMain->getSelection();
	
	Queue* q = g_queues[m_queue];
	
	q->lock();
	foreach(int x, sel)
	{
		if(!files.isEmpty())
			files += '\n';
		files += "file://";
		files += q->at(x)->dataPath(true).toUtf8();
	}
	q->unlock();

	QDataStream stream(&encodedData, QIODevice::WriteOnly);
	stream << m_queue << sel;
	
	mimeData->setData("application/x-fatrat-transfer", encodedData);
	mimeData->setData("text/uri-list", files);
	
	return mimeData;
}

void ProgressDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	if(index.column() == 1)
	{
		TransfersModel* model = (TransfersModel*) index.internalPointer();
		QStyleOptionProgressBarV2 opts;
		const int row = index.row();
		
		if(row < model->m_lastData.size())
		{
			if(model->m_lastData[row].size != "?")
				opts.text = QString("%1%").arg(model->m_lastData[row].fProgress, 0, 'f', 1);
			else
				opts.text = "?";
			
			opts.maximum = 1000;
			opts.minimum = 0;
			opts.progress = int( model->m_lastData[row].fProgress*10 );
			opts.rect = option.rect;
			opts.textVisible = true;
			opts.state = QStyle::State_Enabled;
			
			QApplication::style()->drawControl(QStyle::CE_ProgressBar, &opts, painter);
			
		}
	}
	else
		QItemDelegate::paint(painter, option, index);
}
