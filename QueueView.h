#ifndef QUEUEVIEW_H
#define QUEUEVIEW_H
#include <QTreeWidget>
#include <QMimeData>
#include <QDragMoveEvent>

#include "tooltips/QueueToolTip.h"

class QueueView : public QTreeWidget
{
Q_OBJECT
public:
	QueueView(QWidget* parent);
	bool dropMimeData(QTreeWidgetItem* parent, int index, const QMimeData* data, Qt::DropAction action);
	QStringList mimeTypes() const;
	void mouseMoveEvent(QMouseEvent* event);
	void setCurrentRow(int row);
private:
	QueueToolTip* m_status;
};

#endif
