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
*/

#include "fatrat.h"
#include "Settings.h"
#include "QueueMgr.h"
#include "RuntimeException.h"
#include <QSettings>

using namespace std;

extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;
extern QSettings* g_settings;

QueueMgr* QueueMgr::m_instance = 0;

QueueMgr::QueueMgr() : m_nCycle(0), m_down(0), m_up(0)
{
	m_instance = this;
	
	m_timer = new QTimer(this);
	connect(m_timer, SIGNAL(timeout()), this, SLOT(doWork()), Qt::DirectConnection);
	
	connect(TransferNotifier::instance(), SIGNAL(stateChanged(Transfer*,Transfer::State,Transfer::State)), this, SLOT(transferStateChanged(Transfer*,Transfer::State,Transfer::State)));
	
	m_timer->start(1000);
}

void QueueMgr::doWork()
{
	int total[2] = { 0, 0 };
	g_queuesLock.lockForRead();
	
	const bool autoremove = getSettingsValue("autoremove").toBool();
	
	foreach(Queue* q,g_queues)
	{
		int lim_down,lim_up;
		int down,up, active = 0;
		
		Queue::Stats stats;
		
		memset(&stats, 0, sizeof stats);
		
		q->transferLimits(lim_down,lim_up);
		q->speedLimits(down,up);
		
		q->lock();
		
		QList<int> stopList, resumeList;
		
		for(int i=0;i<q->m_transfers.size();i++)
		{
			Transfer* d = q->m_transfers[i];
			int downs,ups;
			Transfer::State state = d->state();
			Transfer::Mode mode = d->mode();
			
			d->updateGraph();
			d->speeds(downs,ups);
			
			if(downs >= 1024 && mode == Transfer::Download)
				d->m_bWorking = true;
			else if(ups >= 1024 && mode == Transfer::Upload)
				d->m_bWorking = true;
			
			stats.down += downs;
			stats.up += ups;
			
			if(state == Transfer::Waiting || state == Transfer::Active)
			{
				int* lim;
				
				if(mode == Transfer::Download || q->m_bUpAsDown)
					lim = &lim_down;
				else
					lim = &lim_up;
				
				if(*lim != 0)
				{
					(*lim)--;
					resumeList << i;
				}
				else
					stopList << i;
			}
			else if(state == Transfer::Completed && autoremove)
			{
				doMove(q, d);
				q->remove(i--, true);
			}
			
			if(d->isActive())
			{
				( (mode == Transfer::Download) ? stats.active_d : stats.active_u) ++;
				active++;
			}
			else if(d->state() == Transfer::Waiting)
				( (mode == Transfer::Download) ? stats.waiting_d : stats.waiting_u) ++;
		}
		
		foreach(int x, stopList)
			q->m_transfers[x]->setState(Transfer::Waiting);
		foreach(int x, resumeList)
			q->m_transfers[x]->setState(Transfer::Active);
		
		total[0] += stats.down;
		total[1] += stats.up;
		
		int size = q->size();
		
		if(size && active)
		{
			float avgd, avgu, supd, supu;
			int curd, curu;
			
			q->autoLimits(curd, curu);
			
			if(!curd)
				curd = down/active;
			else if(down)
			{
				avgd = float(stats.down) / active;
				supd = float(down) / active;
				curd += (supd-avgd)/active;
			}
			else
				curd = 0;
			
			if(!curu)
				curu = up/active;
			else if(up)
			{
				avgu = float(stats.up) / active;
				supu = float(up) / active;
				//qDebug() << "avgu:" << avgu << "supu:" << supu << "->" << (supu-avgu)/active;
				curu += (supu-avgu)/active;
			}
			else
				curu = 0;
			
			if(curd)
			{
				if(curd < 1024)
					curd = 1024;
				else if(curd < down/active)
					curd = down/active;
			}
			if(curd > down)
				curd = down;
			if(curu)
			{
				if(curu < 1024)
					curu = 1024;
				else if(curu < up/active)
				{
					//qDebug() << "Normalizing to" << (up/active) << "active being" << active;
					curu = up/active;
				}
			}
			if(curu > up)
				curu = up;
			
			//qDebug() << "Setting" << curu;
			q->setAutoLimits(curd, curu);
		}
		
		q->m_stats = stats;
		
		q->unlock();
	}
	
	g_queuesLock.unlock();
	
	m_down = total[0];
	m_up = total[1];
	
	if(++m_nCycle > 60)
	{
		m_nCycle = 0;
		Queue::saveQueues();
		g_settings->sync();
	}
}

void QueueMgr::doMove(Queue* q, Transfer* t)
{
	QString whereTo = q->moveDirectory();
	if(whereTo.isEmpty() || t->primaryMode() != Transfer::Download)
		return;
	
	try
	{
		t->setObject(whereTo);
	}
	catch(const RuntimeException& e)
	{
		t->enterLogMessage("QueueMgr", tr("Failed to move the transfer's data: %1").arg(e.what()));
	}
}

void QueueMgr::transferStateChanged(Transfer* t, Transfer::State, Transfer::State now)
{
	const bool autoremove = getSettingsValue("autoremove").toBool();
	if(now == Transfer::Completed)
	{
		if(autoremove)
			return;
		Queue* q = findQueue(t);
		if(q != 0)
			doMove(q, t);
	}
	else if(now == Transfer::Failed)
	{
		bool bRetry = false;
		if(g_settings->value("retryworking", getSettingsDefault("retryworking")).toBool())
			bRetry = t->m_bWorking;
		else if(g_settings->value("retrycount", getSettingsDefault("retrycount")).toInt() > t->m_nRetryCount)
			bRetry = true;
		
		if(bRetry)
			QMetaObject::invokeMethod(t, "retry", Qt::QueuedConnection);
	}
}

Queue* QueueMgr::findQueue(Transfer* t)
{
	QReadLocker l(&g_queuesLock);
	for(int i=0;i<g_queues.size();i++)
	{
		if(g_queues[i]->contains(t))
			return g_queues[i];
	}
	return 0;
}

void QueueMgr::exit()
{
	delete m_timer;
	
	g_queuesLock.lockForRead();
	foreach(Queue* q,g_queues)
	{
		foreach(Transfer* d,q->m_transfers)
		{
			if(d->isActive())
				d->changeActive(false);
		}
	}
	g_queuesLock.unlock();
}
