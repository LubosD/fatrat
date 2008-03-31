#include "ContextListWidget.h"
#include <QContextMenuEvent>

ContextListWidget::ContextListWidget(QWidget* parent)
	: QListWidget(parent)
{
	QAction* action;
	action = m_menu.addAction(tr("Add"));
	connect(action, SIGNAL(triggered()), this, SLOT(addItem()));
	
	action = m_menu.addAction(tr("Edit"));
	connect(action, SIGNAL(triggered()), this, SLOT(editItem()));
	
	action = m_menu.addAction(tr("Delete"));
	connect(action, SIGNAL(triggered()), this, SLOT(deleteItem()));
}

void ContextListWidget::contextMenuEvent(QContextMenuEvent* event)
{
	m_menu.exec(mapToGlobal(event->pos()));
}

void ContextListWidget::addItem()
{
	QListWidgetItem* item = new QListWidgetItem(QIcon(":/fatrat/miscellaneous.png"), QString(), this);
	QListWidget::addItem(item);
	item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
	QListWidget::editItem(item);
}

void ContextListWidget::editItem()
{
	if(QListWidgetItem* item = currentItem())
		QListWidget::editItem(item);
}

void ContextListWidget::deleteItem()
{
	int i = currentRow();
	if(i != -1)
		delete takeItem(i);
}
