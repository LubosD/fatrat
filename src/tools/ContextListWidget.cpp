/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation
with the OpenSSL special exemption.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

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
