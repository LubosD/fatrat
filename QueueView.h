#ifndef QUEUEVIEW_H
#define QUEUEVIEW_H
#include <QListWidget>
#include <QMimeData>
#include <QDragMoveEvent>
#include <iostream>

class QueueView : public QListWidget
{
Q_OBJECT
public:
	QueueView(QWidget* parent) : QListWidget(parent)
	{
	}
	bool dropMimeData(int queueTo, const QMimeData* data, Qt::DropAction action);
	QStringList mimeTypes() const;
};

#endif
