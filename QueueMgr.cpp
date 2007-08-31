#include "QueueMgr.h"
#include <iostream>
#include <QSettings>

using namespace std;

extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;
extern QSettings* g_settings;

QueueMgr::QueueMgr() : m_nCycle(0)
{
}

void QueueMgr::run()
{
	m_timer = new QTimer;
	connect(m_timer, SIGNAL(timeout()), this, SLOT(doWork()), Qt::DirectConnection);
	m_timer->start(1000);
	exec();
	delete m_timer;
}

void QueueMgr::doWork()
{
	//cout << "QueueMgr::doWork()\n";
	g_queuesLock.lockForRead();
	const int threshold = g_settings->value("speedthreshold", int(0)).toInt()*1024;
	const bool autoremove = g_settings->value("autoremove", false).toBool();
	
	foreach(Queue* q,g_queues)
	{
		int running = 0;
		
		int lim_down,lim_up;
		int down,up;
		int downt=0,upt=0;
		
		q->transferLimits(lim_down,lim_up);
		q->speedLimits(down,up);
		
		//foreach(Transfer* d,q->m_transfers)
		for(int i=0;i<q->m_transfers.size();i++)
		{
			Transfer* d = q->m_transfers[i];
			int downs,ups;
			Transfer::State state = d->state();
			Transfer::Mode mode = d->mode();
			
			d->speeds(downs,ups);
			
			downt += downs;
			upt += ups;
			
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
					
					if(!threshold || downt >= threshold || upt >= threshold)
						running++;
				}
				else
					d->setState(Transfer::Waiting);
			}
			else if(state == Transfer::Completed && autoremove)
				q->remove(i--);
		}
		
		if(running > 0 && (down || up))
		{
			//cout << "--- " << running << " active transfers, using " << downt << " B/s, total limit is " << down << endl;
			
			int downl=0,upl=0;
			
			// unused bandwidth
			if(down)
			{
				downl = down/running;
				if(running > 1)
					downl += std::max(down-downt,0)/(running-1);
			}
			if(up)
			{
				upl = up/running;
				if(running > 1)
					upl += std::max(up-upt,0)/(running-1);
			}
			
			//cout << "--- limiting to " << down << endl;
			
			foreach(Transfer* d,q->m_transfers)
			{
				d->setInternalSpeedLimits(downl,upl);
			}
		}
	}
	
	g_queuesLock.unlock();
	
	if(++m_nCycle > 30)
	{
		m_nCycle = 0;
		Queue::saveQueues();
	}
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
