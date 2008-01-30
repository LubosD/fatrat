#ifndef QUEUETOOLTIP_H
#define QUEUETOOLTIP_H
#include "BaseToolTip.h"

#include "Queue.h"

class QueueToolTip : public BaseToolTip
{
public:
	QueueToolTip(QWidget* parent, Queue* queue);
	Queue* getQueue() const { return m_queue; }
	void refresh();
private:
	Queue* m_queue;
};

#endif
