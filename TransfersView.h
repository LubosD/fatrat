#ifndef TRANSFERSVIEW_H
#define TRANSFERSVIEW_H
#include <QTreeView>
#include <QPixmap>
#include <QPainter>
#include <QHeaderView>
#include <QDrag>

class TransfersView : public QTreeView
{
Q_OBJECT
public:
	TransfersView(QWidget* parent) : QTreeView(parent)
	{
	}
protected:
	virtual void startDrag(Qt::DropActions supportedActions) // from Qt
	{
		QModelIndexList indexes = selectedIndexes();
		if(indexes.count() > 0)
		{
			QMimeData *data = model()->mimeData(indexes);
			if(!data)
				return;
			
			QRect rect;
			QPixmap pixmap(100, 100);
			QPainter painter;
			QString text;
			
			pixmap.fill(Qt::transparent);
			painter.begin(&pixmap);
			
			text = tr("%1 transfers").arg(indexes.size()/header()->count());
			painter.drawText(rect, 0, text, &rect);
			
			rect.moveTo(5, 5);
			QRect around = rect.adjusted(-5, -5, 5, 5);
			painter.fillRect(around, Qt::white);
			
			painter.drawText(rect, 0, text, 0);
			
			painter.setPen(Qt::darkGray);
			painter.drawRect(around);
			
			painter.end();
			
			QDrag *drag = new QDrag(this);
			drag->setPixmap(pixmap);
			drag->setMimeData(data);
			drag->setHotSpot(QPoint(-20, -20));
			
			drag->start(supportedActions);
		}
	}
};

#endif

