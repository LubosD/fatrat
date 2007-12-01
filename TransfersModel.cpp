#include <QApplication>
#include <QPainter>
#include <QMimeData>

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
	qDeleteAll(m_states,m_states+8);
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
			return tr("Speed");
		case 4:
			return tr("Time left");
		case 5:
			return tr("Message");
		}
	}

	return QVariant();
}

int TransfersModel::rowCount(const QModelIndex &parent) const
{
	int count = 0;
	if(!parent.isValid())
	{
		g_queuesLock.lockForRead();
		
		//cout << "Parent isn't valid\n";
		
		if(m_queue < g_queues.size() && m_queue >= 0)
			count = g_queues[m_queue]->size();
		
		g_queuesLock.unlock();
	}
	
	//cout << "Returning row count " << count << endl;
	return count;
}

void TransfersModel::refresh()
{
	int count = 0;
	g_queuesLock.lockForRead();
	
	if(m_queue < g_queues.size() && m_queue >= 0)
		count = g_queues[m_queue]->size();
	
	//cout << "Current row count: " << count << endl;
		
	g_queuesLock.unlock();
	
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
	return 6;
}

QVariant TransfersModel::data(const QModelIndex &index, int role) const
{
	QVariant rval;
	
	if(role == Qt::DisplayRole)
	{
		g_queuesLock.lockForRead();
			
		if(m_queue < g_queues.size())
		{
			Transfer* d;
			Queue* q = g_queues[m_queue];
			q->lock();
			
			d = q->at(index.row());
			if(!d)
			{
				q->unlock();
				return QVariant();
			}
			
			switch(index.column())
			{
			//case 0:
				//rval = Transfer::state2string(d->state()); break;
			//	rval = *m_states[d->state()]; break;
			case 0:
				rval = d->name(); break;
			case 1:
				rval = (d->total()) ? QString("%1%").arg(100.0/d->total()*d->done(), 0, 'f', 1) : QVariant(); break;
			case 2:
				rval = (d->total()) ? formatSize(d->total()) : QString("?"); break;
			case 3:
				if(d->isActive())
				{
					QString s;
					int down,up;
					d->speeds(down,up);
					
					if(down)
						s = QString("%1 kB/s down").arg(double(down)/1024.f, 0, 'f', 1);
					if(up)
					{
						if(!s.isEmpty())
							s += " | ";
						s += QString("%1 kB/s up").arg(double(up)/1024.f, 0, 'f', 1);
					}
					
					rval = s;
				}
				break;
			
			case 4: // time left
				if(d->isActive() && d->total())
				{
					QString s;
					int down,up;
					qulonglong totransfer = d->total() - d->done();
					
					d->speeds(down,up);
					
					if(down)
						rval = formatTime(totransfer/down);
					else if(up && d->primaryMode() == Transfer::Upload)
						rval = formatTime(totransfer/up);
				}
				break;
			case 5:
				rval = d->message(); break;
			}
			q->unlock();
		}
		
		g_queuesLock.unlock();
	}
	else if(role == Qt::DecorationRole)
	{
		if(index.column() == 0)
		{
			g_queuesLock.lockForRead();
			Transfer* d;
			Transfer::State state;
			Queue* q = g_queues[m_queue];
			q->lock();
			
			d = q->at(index.row());
			if(d == 0)
			{
				q->unlock();
				return QVariant();
			}
			
			state = d->state();
			if(d->mode() == Transfer::Upload)
			{
				if(state == Transfer::Completed && d->primaryMode() == Transfer::Download)
					rval = *m_states[5]; // an exception for download-oriented transfers
				else
					rval = *m_states[state+6];
			}
			else
				rval = *m_states[state];
			
			q->unlock();
			g_queuesLock.unlock();
		}
	}
	else if(role == Qt::SizeHintRole)
	{
		rval = QSize(50, 50);
	}

	return rval;
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
	QMimeData *mimeData = new QMimeData;
	QByteArray encodedData;
	QList<int> sel = g_wndMain->getSelection();

	QDataStream stream(&encodedData, QIODevice::WriteOnly);
	stream << m_queue << sel;
	
	mimeData->setData("application/x-fatrat-transfer", encodedData);
	
	return mimeData;
}

void ProgressDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	//cout << "ProgressDelegate::paint()\n";
	
	if(index.column() == 1)
	{
		TransfersModel* model = (TransfersModel*) index.internalPointer();
		QStyleOptionProgressBarV2 opts;
		
		g_queuesLock.lockForRead();
		if(model->m_queue < g_queues.size())
		{
			Transfer* d;
			Queue* q = g_queues[model->m_queue];
			q->lock();
			
			d = q->at(index.row());
			if(!d)
			{
				q->unlock();
				return;
			}
			
			if(d->total())
				opts.text = QString("%1%").arg(100.0/d->total()*d->done(), 0, 'f', 1);
			else
				opts.text = "?";
			
			opts.maximum = 1000;
			opts.minimum = 0;
			opts.progress = int( 1000.0/d->total()*d->done() );
			opts.rect = option.rect;
			opts.textVisible = true;
			
			QApplication::style()->drawControl(QStyle::CE_ProgressBar, &opts, painter);
			
			q->unlock();
		}
		g_queuesLock.unlock();
	}
	else
		QItemDelegate::paint(painter, option, index);
}
