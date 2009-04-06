/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation
with the OpenSSL special exemption.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "DbusImpl.h"
#include "Queue.h"
#include "MainWindow.h"
#include "fatrat.h"
#include "RuntimeException.h"
#include "Settings.h"
#include <QRegExp>
#include <QReadWriteLock>
#include <QtDBus/QtDBus>
#include <QtDebug>

extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;
extern QVector<EngineEntry> g_enginesDownload;
extern QVector<EngineEntry> g_enginesUpload;

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
		
		if(!getSettingsValue("link_separator").toInt())
			listUris = uris.split('\n', QString::SkipEmptyParts);
		else
			listUris = uris.split(QRegExp("\\s+"), QString::SkipEmptyParts);
		
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
			for(int i=0;i<g_enginesDownload.size();i++)
			{
				if(className == g_enginesDownload[i].shortName)
				{
					_class = &g_enginesDownload[i];
					break;
				}
			}
			
			for(int i=0;i<g_enginesUpload.size();i++)
			{
				if(className == g_enginesUpload[i].shortName)
				{
					_class = &g_enginesUpload[i];
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

void DbusImpl::addTransfersNonInteractive2(QString uris, QString target, QString className, int queueID, QString* resp)
{
	QString r = addTransfersNonInteractive(uris, target, className, queueID);
	
	if(r.isEmpty())
		r = "OK";
	*resp = r;
}

// This is a one big workaround for a bug far far from here
// 1) Qt cannot handle return values via Qt::QueuedConnection ->
// 2) QHttp doesn't work when used outside the main thread ->
// 3) TorrentDownload cannot download a torrent ->
// 4) We cannot used a non-queued connection ->
// 5) Hack it around like this

QString DbusImpl::addTransfers(QString uris, QString target, QString className, int queueID)
{
	QString response;
	QMetaObject::invokeMethod(this,
			"addTransfersNonInteractive2", Qt::QueuedConnection,
			Q_ARG(QString, uris), Q_ARG(QString, target),
			Q_ARG(QString, className), Q_ARG(int, queueID), Q_ARG(QString*, &response));
	
	while(response.isEmpty())
		Sleeper::msleep(100);
	Sleeper::msleep(100);
	if(response == "OK")
		return QString();
	else
		return response;
}

