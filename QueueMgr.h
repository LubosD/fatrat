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
	
	static QueueMgr* instance() { return m_instance; }
	
	inline int totalDown() const { return m_down; }
	inline int totalUp() const { return m_up; }
public slots:
	void doWork();
	void transferStateChanged(Transfer*,Transfer::State,Transfer::State);
private:
	static QueueMgr* m_instance;
	QTimer* m_timer;
	int m_nCycle;
	int m_down, m_up;
};

#endif
