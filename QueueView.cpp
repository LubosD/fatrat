#include "QueueView.h"
#include "Queue.h"
#include "MainWindow.h"
#include "fatrat.h"

extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;

bool QueueView::dropMimeData(int queueTo, const QMimeData* data, Qt::DropAction action)
{
	if(action == Qt::IgnoreAction)
		return true;

	if(data->hasFormat("application/x-fatrat-transfer"))
	{
		g_queuesLock.lockForRead();
		
		QByteArray encodedData = data->data("application/x-fatrat-transfer");
		QDataStream stream(&encodedData, QIODevice::ReadOnly);
		int queueFrom;
		QList<int> transfers;
		QList<Transfer*> objects;
		Queue* q;
		
		stream >> queueFrom >> transfers;
		
		if(queueFrom != queueTo)
		{
			q = g_queues[queueFrom];
			q->lockW();
			
			for(int i=0;i<transfers.size();i++)
				objects << q->take(transfers[i]-i, false);
			
			q->unlock();
			
			q = g_queues[queueTo];
			q->add(objects);
		}
		
		g_queuesLock.unlock();
	}
	else if(data->hasFormat("text/plain"))
	{
		MainWindow* wnd = (MainWindow*) getMainWindow();
		wnd->addTransfer(data->data("text/plain"), Transfer::ModeInvalid, QString(), queueTo);
	}
	else
		return false;
	return true;
}

QStringList QueueView::mimeTypes() const
{
	return QStringList() << "application/x-fatrat-transfer" << "text/plain";
}

void QueueView::mouseMoveEvent(QMouseEvent* event)
{
	QListWidgetItem* item = itemAt(event->pos());
	
	if(!item)
	{
		if(m_status != 0)
		{
			m_status->deleteLater();
			m_status = 0;
		}
	}
	else
	{
		Queue* q = static_cast<Queue*>( item->data(Qt::UserRole).value<void*>() );
		
		if(!m_status || q != m_status->getQueue())
		{
			if(m_status)
				m_status->deleteLater();
			m_status = new QueueStatus(getMainWindow(), q);
		}
		
		m_status->move(mapToGlobal(event->pos()) + QPoint(25, 25));
	}
}
