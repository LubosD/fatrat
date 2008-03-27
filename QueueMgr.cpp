#include "fatrat.h"
#include "QueueMgr.h"
#include <iostream>
#include <QSettings>

using namespace std;

extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;
extern QSettings* g_settings;

QueueMgr* QueueMgr::m_instance = 0;

QueueMgr::QueueMgr() : m_nCycle(0), m_down(0), m_up(0)
{
	m_instance = this;
}

void QueueMgr::run()
{
	m_timer = new QTimer;
	connect(m_timer, SIGNAL(timeout()), this, SLOT(doWork()), Qt::DirectConnection);
	
	connect(TransferNotifier::instance(), SIGNAL(stateChanged(Transfer*,Transfer::State,Transfer::State)), this, SLOT(transferStateChanged(Transfer*,Transfer::State,Transfer::State)));
	
	m_timer->start(1000);
	exec();
	delete m_timer;
}

void QueueMgr::doWork()
{
	int total[2] = { 0, 0 };
	g_queuesLock.lockForRead();
	//const int threshold = g_settings->value("speedthreshold", getSettingsDefault("speedthreshold")).toInt()*1024;
	const bool autoremove = g_settings->value("autoremove", getSettingsDefault("autoremove")).toBool();
	
	foreach(Queue* q,g_queues)
	{
		int lim_down,lim_up;
		int down,up, active = 0;
		
		Queue::Stats stats;
		
		memset(&stats, 0, sizeof stats);
		
		q->transferLimits(lim_down,lim_up);
		q->speedLimits(down,up);
		
		q->lock();
		
		for(int i=0;i<q->m_transfers.size();i++)
		{
			Transfer* d = q->m_transfers[i];
			int downs,ups;
			Transfer::State state = d->state();
			Transfer::Mode mode = d->mode();
			
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
					d->setState(Transfer::Active);
				}
				else
					d->setState(Transfer::Waiting);
				active++;
			}
			else if(state == Transfer::Completed && autoremove)
			{
				q->remove(i--, true);
			}
			
			if(d->isActive())
				( (mode == Transfer::Download) ? stats.active_d : stats.active_u) ++;
			else if(d->state() == Transfer::Waiting)
				( (mode == Transfer::Download) ? stats.waiting_d : stats.waiting_u) ++;
		}
		
		total[0] += stats.down;
		total[1] += stats.up;
		
		int downl = 0, upl = 0;
		int size = q->size();
		
		if(size)
		{
			/*downl = down/size;
			upl = up/size;
			
			qDebug() << "UpL:" << up << "Up:" << stats.up;
			qDebug() << "Upl 1:" << upl;
			
			downl += std::max(down-stats.down,0) /size;
			upl += std::max(up-stats.up,0) /size;
			
			qDebug() << "Upl 2:" << upl;
			qDebug() << "";
			
			downl = std::min(down, downl);
			upl = std::min(up, upl);*/
			
			if(down && active)
			{
				//downl = down;
				downl = down/active + std::max(down-stats.down, 0);
				downl = std::min(down, downl);
			}
			
			if(up && active)
			{
				//upl = up;
				
				//qDebug() << "Upl 1:" << upl;
				
				//upl -= std::max(stats.up-up, 0);
				
				//qDebug() << "Upl 2:" << upl;
				//qDebug() << "";
				
				//upl = std::max(down/size, upl);
				upl = up/active + std::max(up-stats.up, 0);
				upl = std::min(up, upl);
				qDebug() << "UpL:" << up << "Up:" << stats.up;
			}
		}
		
		foreach(Transfer* d,q->m_transfers)
		{
			if(d->isActive())
				d->setInternalSpeedLimits(downl,upl);
		}
		
		q->m_stats = stats;
		
		q->unlock();
	}
	
	g_queuesLock.unlock();
	
	m_down = total[0];
	m_up = total[1];
	
	if(++m_nCycle > 30)
	{
		m_nCycle = 0;
		Queue::saveQueues();
		g_settings->sync();
	}
}

void QueueMgr::transferStateChanged(Transfer* t, Transfer::State, Transfer::State now)
{
	if(now != Transfer::Failed)
		return;
	
	bool bRetry = false;
	if(g_settings->value("retryworking", getSettingsDefault("retryworking")).toBool())
		bRetry = t->m_bWorking;
	else if(g_settings->value("retrycount", getSettingsDefault("retrycount")).toInt() > t->m_nRetryCount)
		bRetry = true;
	
	if(bRetry)
		QMetaObject::invokeMethod(t, "retry", Qt::QueuedConnection);
}

void QueueMgr::exit()
{
	quit();
	wait();
	
	g_queuesLock.lockForRead();
	foreach(Queue* q,g_queues)
	{
		foreach(Transfer* d,q->m_transfers)
		{
			d->setState(Transfer::Paused);
		}
	}
	g_queuesLock.unlock();
}
