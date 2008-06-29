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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
USA.
*/

#include "DbusImpl.h"
#include "Queue.h"
#include "MainWindow.h"
#include "fatrat.h"
#include "RuntimeException.h"
#include <QReadWriteLock>
#include <QtDBus/QtDBus>
#include <QtDebug>

extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;

DbusImpl* DbusImpl::m_instance = 0;

DbusImpl::DbusImpl()
{
	m_instance = this;
}

void DbusImpl::addTransfers(QString uris)
{
	MainWindow* wnd = (MainWindow*) getMainWindow();
	wnd->addTransfer(uris);
}

QString DbusImpl::addTransfersNonInteractive(QString uris, QString target, QString className, int queueID)
{
	QReadLocker locker(&g_queuesLock);
	
	try
	{
		QStringList listUris;
		
		if(uris.isEmpty())
			throw RuntimeException("No URIs were passed");
		
		listUris = uris.split('\n');
		
		if(queueID < 0 || queueID >= g_queues.size())
			throw RuntimeException("queueID is out of range");
	
		const EngineEntry* _class = 0;
		if(className == "auto")
		{
			Transfer::BestEngine eng;
			
			eng = Transfer::bestEngine(listUris[0], Transfer::Download);
			if(eng.nClass < 0)
				eng = Transfer::bestEngine(target, Transfer::Upload);
			
			if(eng.nClass < 0)
				throw RuntimeException("The URI wasn't accepted by any class");
			else
				_class = eng.engine;
		}
		else
		{
			const EngineEntry* entries;
			
			entries = Transfer::engines(Transfer::Download);
			for(int i=0;entries[i].shortName;i++)
			{
				if(className == entries[i].shortName)
				{
					_class = &entries[i];
					break;
				}
			}
			
			entries = Transfer::engines(Transfer::Upload);
			for(int i=0;entries[i].shortName;i++)
			{
				if(className == entries[i].shortName)
				{
					_class = &entries[i];
					break;
				}
			}
			
			if(!_class)
				throw RuntimeException("className doesn't represent any known class");
		}
		
		foreach(QString uri, listUris)
		{
			Transfer* t = _class->lpfnCreate();
			
			if(!t)
				throw RuntimeException("Failed to create an instance of the chosen class");
			
			try
			{
				t->init(uri, target);
				t->setState(Transfer::Waiting);
			}
			catch(...)
			{
				delete t;
				throw;
			}
			
			g_queues[queueID]->add(t);
		}
	}
	catch(const RuntimeException& e)
	{
		qDebug() << "DbusImpl::addTransfersNonInteractive():" << e.what();
		return e.what();
	}
	return QString();
}

QStringList DbusImpl::getQueues()
{
	QStringList result;
	g_queuesLock.lockForRead();
	
	foreach(Queue* q, g_queues)
		result << q->name();
	
	g_queuesLock.unlock();
	return result;
}

