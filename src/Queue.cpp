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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#include "fatrat.h"
#include "Queue.h"
#include "QueueMgr.h"
#include "Settings.h"
#include "engines/PlaceholderTransfer.h"
#include <QList>
#include <QReadWriteLock>
#include <QDir>
#include <QFile>
#include <QDomDocument>
#include <QtDebug>

using namespace std;

QList<Queue*> g_queues;
QReadWriteLock g_queuesLock(QReadWriteLock::Recursive);

bool Queue::m_bLoaded = false;

Queue::Queue()
	: m_nDownLimit(0), m_nUpLimit(0), m_nDownTransferLimit(1), m_nUpTransferLimit(1),
	m_nDownAuto(0), m_nUpAuto(0), m_bUpAsDown(false), m_lock(QReadWriteLock::Recursive)
{
	memset(&m_stats, 0, sizeof m_stats);
	m_uuid = QUuid::createUuid();
	m_strDefaultDirectory = QDir::homePath();
}

Queue::~Queue()
{
	QWriteLocker l(&m_lock);
	qDebug() << "Queue::~Queue()";
	qDeleteAll(m_transfers);
}

void Queue::unloadQueues()
{
	qDebug() << "Queue::unloadQueues()";
	qDeleteAll(g_queues);
}

void Queue::stopQueues()
{
	QReadLocker l(&g_queuesLock);
	for(int i=0;i<g_queues.size();i++)
	{
		Queue* q = g_queues[i];

		q->lock();
		for(int j=0;j<q->size();j++)
		{
			Transfer* t = q->at(j);
			if(t->isActive())
				t->changeActive(false);
		}
		q->unlock();
	}
}

void Queue::loadQueues()
{
	QDomDocument doc;
	QFile file;
	QDir dir = QDir::home();
	
	dir.mkpath(".local/share/fatrat");
	if(!dir.cd(".local/share/fatrat"))
		return;
	file.setFileName(dir.absoluteFilePath("queues.xml"));
	
	QString errmsg;
	if(!file.open(QIODevice::ReadOnly) || !doc.setContent(&file, false, &errmsg))
	{
		qDebug() << "Failed to open " << file.fileName();
		if(!errmsg.isEmpty())
			qDebug() << "PARSE ERROR!" << errmsg;
		
		// default queue for new users
		Queue* q = new Queue;
		q->setName(QObject::tr("Main queue"));
		g_queues << q;
	}
	else
	{
		g_queuesLock.lockForWrite();
		qDeleteAll(g_queues);
		
		qDebug() << "Loading queues";
		
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
				pQueue->m_uuid = QUuid( n.attribute("uuid", pQueue->m_uuid.toString()) );
				pQueue->m_strDefaultDirectory = n.attribute("defaultdir", pQueue->m_strDefaultDirectory);
				pQueue->m_strMoveDirectory = n.attribute("movedir");
				
				pQueue->loadQueue(n);
				g_queues << pQueue;
			}
			n = n.nextSiblingElement("queue");
		}
		
		g_queuesLock.unlock();
	}

	m_bLoaded = true;
}

void Queue::BackgroundSaver::run()
{
	Queue::saveQueues();
	if (getSettingsValue("queue_synconwrite").toBool())
		sync();
}

void Queue::saveQueuesAsync()
{
	BackgroundSaver* t = new BackgroundSaver;
	connect(t, SIGNAL(finished()), t, SLOT(deleteLater()));
	t->start();
}

void Queue::saveQueues()
{
	if (!m_bLoaded)
	{
		qDebug() << "Not saving queues as they haven't been loaded yet.";
		return;
	}

	QDomDocument doc;
	QDomElement root;
	QFile file;
	QDir dir = QDir::home();
	
	if(!dir.cd(".local/share/fatrat"))
		return;
	file.setFileName(dir.filePath("queues.xml.new"));
	
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
		elem.setAttribute("uuid",q->m_uuid.toString());
		elem.setAttribute("defaultdir",q->m_strDefaultDirectory);
		elem.setAttribute("movedir",q->m_strMoveDirectory);
		
		q->saveQueue(elem,doc);
		root.appendChild(elem);
	}
	
	g_queuesLock.unlock();
	if (file.write(doc.toByteArray()) == -1 || !file.flush())
		Logger::global()->enterLogMessage(tr("Queue"), tr("Failed to write the queue file!"));
	else {
		file.close();

		QByteArray path = (dir.path() + "/queues.xml.new").toUtf8();
		QByteArray dpath = (dir.path() + "/queues.xml").toUtf8();
		
		qDebug() << "Saving queue to" << dpath;
		rename(path.data(), dpath.data());
	}
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
			m_transfers << d;
		}
		else
		{
			qDebug() << "***ERROR*** Unable to createInstance " << n.attribute("class").toAscii();

			d = new PlaceholderTransfer(n.attribute("class"));
			d->load(n);
			m_transfers << d;
		}
		
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

int Queue::moveDown(int n, bool nolock)
{
	if(m_transfers.size()>n+1)
	{
		if (!nolock)
			m_lock.lockForWrite();
		m_transfers.swap(n,n+1);
		if (!nolock)
			m_lock.unlock();
		
		return n+1;
	}
	else
		return n;
}

int Queue::moveUp(int n, bool nolock)
{
	if(n > 0)
	{
		if (!nolock)
			m_lock.lockForWrite();
		m_transfers.swap(n-1,n);
		if (!nolock)
			m_lock.unlock();
		return n-1;
	}
	else
		return n;
}

void Queue::moveToPos(int from, int to, bool nolock)
{
	Transfer* t;
	
	if(to > from)
		to--;
	
	if (!nolock)
		m_lock.lockForWrite();
	t = m_transfers.takeAt(from);
	m_transfers.insert(to, t);
	if (!nolock)
		m_lock.unlock();
}

void Queue::moveToTop(int n, bool nolock)
{
	if (!nolock)
		m_lock.lockForWrite();
	m_transfers.prepend(m_transfers.takeAt(n));
	if (!nolock)
		m_lock.unlock();
}

void Queue::moveToBottom(int n, bool nolock)
{
	if (!nolock)
		m_lock.lockForWrite();
	m_transfers.append(m_transfers.takeAt(n));
	if (!nolock)
		m_lock.unlock();
}

Transfer* Queue::take(int n, bool nolock)
{
	Transfer* d = 0;
	
	if(!nolock)
		m_lock.lockForWrite();
	if(n < size() && n >= 0)
		d = m_transfers.takeAt(n);
	if(!nolock)
		m_lock.unlock();
	
	return d;
}

void Queue::remove(int n, bool nolock)
{
	Transfer* d = take(n, nolock);
	
	if(d->isActive())
		d->setState(Transfer::Paused);
	d->deleteLater();
}

void Queue::removeWithData(int n, bool nolock)
{
	Transfer* d = take(n, nolock);
	
	if(d->isActive())
		d->setState(Transfer::Paused);
	
	QString path = d->dataPath(true);
	
	if(!path.isEmpty() && d->primaryMode() == Transfer::Download)
		recursiveRemove(path);
	
	d->deleteLater();
}

void Queue::setAutoLimits(int down, int up)
{
	m_nDownAuto = down;
	m_nUpAuto = up;
	
	foreach(Transfer* d, m_transfers)
	{
		if(!d->isActive())
			continue;
		d->setInternalSpeedLimits(down, up);
	}
}

void Queue::setName(QString name)
{
	QWriteLocker l(&m_lock);
	m_strName = name;
}
QString Queue::name() const
{
	QReadLocker l(&m_lock);
	return m_strName;
}

void Queue::setDefaultDirectory(QString path)
{
	QWriteLocker l(&m_lock);
	m_strDefaultDirectory = path;
}

QString Queue::defaultDirectory() const
{
	QReadLocker l(&m_lock);
	return m_strDefaultDirectory;
}

void Queue::setMoveDirectory(QString path)
{
	QWriteLocker l(&m_lock);
	m_strMoveDirectory = path;
}

QString Queue::moveDirectory() const
{
	QReadLocker l(&m_lock);
	return m_strMoveDirectory;
}

bool Queue::contains(Transfer* t) const
{
	QReadLocker l(&m_lock);
	return m_transfers.contains(t);
}

bool Queue::replace(Transfer* old, Transfer* _new)
{
	QWriteLocker l(&m_lock);
	int i = m_transfers.indexOf(old);
	if (i == -1)
		return false;
	Transfer* t = m_transfers[i];
	m_transfers[i] = _new;
	t->deleteLater();
	return true;
}

bool Queue::replace(Transfer* old, QList<Transfer*> _new)
{
	QWriteLocker l(&m_lock);
	int i = m_transfers.indexOf(old);
	if (i == -1)
		return false;
	m_transfers.takeAt(i)->deleteLater();

	for (int j = 0; j < _new.size(); j++)
		m_transfers.insert(i+j, _new[j]);
	return true;
}

void Queue::stopAll()
{
	QReadLocker l(&m_lock);
	for(int j=0;j<size();j++)
	{
		Transfer* t = at(j);
		if(t->isActive())
			t->setState(Transfer::Paused);
	}
}

void Queue::resumeAll()
{
	QReadLocker l(&m_lock);
	for(int j=0;j<size();j++)
	{
		Transfer* t = at(j);
		Transfer::State state = t->state();
		if(state == Transfer::Paused || state == Transfer::Failed)
			t->setState(Transfer::Active);
	}
}

void Queue::updateGraph()
{
	int downq = 0, upq = 0;

	lock();

	for(int i=0;i<size();i++)
	{
		int up,down;
		at(i)->speeds(down,up);

		downq += down;
		upq += up;
	}

	unlock();

	if(m_qSpeedData.size() >= getSettingsValue("graphminutes").toInt()*60)
		m_qSpeedData.dequeue();
	m_qSpeedData.enqueue(QPair<int,int>(downq,upq));
}
