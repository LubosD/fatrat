#ifndef _QUEUEMGR_H
#define _QUEUEMGR_H
#include <QThread>
#include <QTimer>
#include "Queue.h"
#include <QSettings>

class QueueMgr : public QThread
{
Q_OBJECT
public:
	QueueMgr();
	virtual void run();
	void exit();
public slots:
	void doWork();
private:
	QTimer* m_timer;
	int m_nCycle;
};

#endif
