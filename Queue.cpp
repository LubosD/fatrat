#include "Queue.h"
#include "QueueMgr.h"
#include <QList>
#include <QReadWriteLock>
#include <QDir>
#include <QFile>
#include <QDomDocument>
#include <iostream>
#include <QtDebug>

using namespace std;

QList<Queue*> g_queues;
QReadWriteLock g_queuesLock;
QueueMgr g_qmgr;

Queue::Queue()
	: m_nDownLimit(0), m_nUpLimit(0), m_nDownTransferLimit(0), m_nUpTransferLimit(0), m_bUpAsDown(false)
{
}

Queue::~Queue()
{
	qDebug() << "Queue::~Queue()";
	qDeleteAll(m_transfers);
}

void Queue::unloadQueues()
{
	qDebug() << "Queue::unloadQueues()";
	//g_queuesLock.lockForWrite();
	qDeleteAll(g_queues);
	//g_queuesLock.unlock();
}

void Queue::loadQueues()
{
	QDomDocument doc;
	QFile file;
	QDir dir = QDir::home();
	
	dir.mkdir(".fatrat");
	if(!dir.cd(".fatrat"))
		return;
	file.setFileName(dir.filePath("queues.xml"));
	if(!file.open(QIODevice::ReadOnly) || !doc.setContent(&file))
	{
		// default queue for new users
		Queue* q = new Queue;
		q->setName(QObject::tr("Main queue"));
		g_queues << q;
	}
	else
	{
		g_queuesLock.lockForWrite();
		qDeleteAll(g_queues);
		
		cout << "Loading queues\n";
		
		QDomElement n = doc.documentElement().firstChildElement("queue");
		while(!n.isNull())
		{
			if(!n.hasAttribute("name"))
				continue;
			else
			{
				Queue* pQueue = new Queue;
				
				pQueue->m_strName = n.attribute("name");
				pQueue->m_nDownLimit = n.attribute("downlimit").toInt();
				pQueue->m_nUpLimit = n.attribute("uplimit").toInt();
				pQueue->m_nDownTransferLimit = n.attribute("dtranslimit").toInt();
				pQueue->m_nUpTransferLimit = n.attribute("utranslimit").toInt();
				pQueue->m_bUpAsDown = n.attribute("upasdown").toInt() != 0;
				
				pQueue->loadQueue(n);
				g_queues << pQueue;
			}
			n = n.nextSiblingElement("queue");
		}
		
		g_queuesLock.unlock();
	}
}

void Queue::saveQueues()
{
	QDomDocument doc;
	QDomElement root;
	QFile file;
	QDir dir = QDir::home();
	
	if(!dir.cd(".fatrat"))
		return;
	file.setFileName(dir.filePath("queues.xml"));
	
	if(!file.open(QIODevice::WriteOnly))
		return;
	
	root = doc.createElement("fatrat");
	doc.appendChild(root);
	
	g_queuesLock.lockForRead();
	
	foreach(Queue* q, g_queues)
	{
		QDomElement elem = doc.createElement("queue");
		elem.setAttribute("name",q->m_strName);
		elem.setAttribute("downlimit",QString::number(q->m_nDownLimit));
		elem.setAttribute("uplimit",QString::number(q->m_nUpLimit));
		elem.setAttribute("dtranslimit",QString::number(q->m_nDownTransferLimit));
		elem.setAttribute("utranslimit",QString::number(q->m_nUpTransferLimit));
		elem.setAttribute("upasdown",QString::number(q->m_bUpAsDown));
		
		q->saveQueue(elem,doc);
		root.appendChild(elem);
	}
	
	g_queuesLock.unlock();
	file.write(doc.toByteArray());
}

void Queue::loadQueue(const QDomNode& node)
{
	m_lock.lockForWrite();
	
	qDeleteAll(m_transfers);
	
	QDomElement n = node.firstChildElement("download");
	while(!n.isNull())
	{
		QDomElement e = n.firstChildElement("param");
		QMap<QString,QString> map;
		Transfer* d;
		
		//cout << "Creating instance\n";
		
		d = Transfer::createInstance(n.attribute("class"));
		
		if(d != 0)
		{
			/*while(!e.isNull())
			{
				if(e.hasAttribute("name"))
					map[e.attribute("name")] = e.text();
				
				e = e.nextSiblingElement("param");
			}
			*/
			d->load(n);
			//cout << "Loaded instance\n";
			m_transfers << d;
		}
		else
			cerr << "***ERROR*** Unable to createInstance '" << n.attribute("class").toAscii().constData() << "'\n";
		
		n = n.nextSiblingElement("download");
	}
	
	m_lock.unlock();
}

void Queue::saveQueue(QDomNode& node,QDomDocument& doc)
{
	lock();
	
	foreach(Transfer* d,m_transfers)
	{
		QDomElement elem = doc.createElement("download");
		
		d->save(doc, elem);
		elem.setAttribute("class",d->myClass());
		
		node.appendChild(elem);
	}
	
	unlock();
}

int Queue::size()
{
	//cout << "Queue size: " << m_transfers.size() << endl;
	return m_transfers.size();
}

Transfer* Queue::at(int r)
{
	if(r < 0 || r >= m_transfers.size())
		return 0;
	else
		return m_transfers[r];
}

void Queue::add(Transfer* d)
{
	m_lock.lockForWrite();
	m_transfers << d;
	m_lock.unlock();
}

void Queue::add(QList<Transfer*> d)
{
	m_lock.lockForWrite();
	m_transfers << d;
	m_lock.unlock();
}

void Queue::moveDown(int n)
{
	if(m_transfers.size()>n+1)
	{
		m_lock.lockForWrite();
		m_transfers.swap(n,n+1);
		m_lock.unlock();
	}
}

void Queue::moveUp(int n)
{
	if(n > 0)
	{
		m_lock.lockForWrite();
		m_transfers.swap(n-1,n);
		m_lock.unlock();
	}
}

void Queue::moveToTop(int n)
{
	m_lock.lockForWrite();
	m_transfers.prepend(m_transfers.takeAt(n));
	m_lock.unlock();
}

void Queue::moveToBottom(int n)
{
	m_lock.lockForWrite();
	m_transfers.append(m_transfers.takeAt(n));
	m_lock.unlock();
}

void Queue::remove(int n, bool nolock)
{
	Transfer* d = 0;
	
	if(!nolock)
		m_lock.lockForWrite();
	if(n < size() && n >= 0)
		d = m_transfers.takeAt(n);
	if(!nolock)
		m_lock.unlock();
	
	if(d->isActive())
		d->setState(Transfer::Paused);
	d->deleteLater();
}



