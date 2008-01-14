#ifndef QUEUEVIEW_H
#define QUEUEVIEW_H
#include <QListWidget>
#include <QMimeData>
#include <QDragMoveEvent>

#include "QueueStatus.h"

class QueueView : public QListWidget
{
Q_OBJECT
public:
	QueueView(QWidget* parent);
	bool dropMimeData(int queueTo, const QMimeData* data, Qt::DropAction action);
	QStringList mimeTypes() const;
	void mouseMoveEvent(QMouseEvent* event);
private:
	QueueStatus* m_status;
};

#endif
