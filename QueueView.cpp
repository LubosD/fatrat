#include "QueueView.h"
#include "Queue.h"

extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;

bool QueueView::dropMimeData(int queueTo, const QMimeData* data, Qt::DropAction action)
{
	std::cout << "Drop at index " << queueTo << std::endl;
	if(action == Qt::IgnoreAction)
		return true;

	if(!data->hasFormat("application/x-fatrat-transfer"))
		return false;
	
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
	
	return true;
}
QStringList QueueView::mimeTypes() const
{
	return QStringList("application/x-fatrat-transfer");
}
