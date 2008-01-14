#ifndef QUEUESTATUS_H
#define QUEUESTATUS_H
#include <QLabel>
#include <QTimer>

#include "Queue.h"

class QueueStatus : public QLabel
{
Q_OBJECT
public:
	QueueStatus(QWidget* parent, Queue* queue);
	Queue* getQueue() const { return m_queue; }
protected:
	void mousePressEvent(QMouseEvent* event);
public slots:
	void refresh();
private:
	Queue* m_queue;
	QTimer m_timer;
};

#endif
