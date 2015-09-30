/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 3 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#include "fatrat.h"
#include "ContextListWidget.h"
#include <QContextMenuEvent>
#include <QFile>
#include <cstring>

ContextListWidget::ContextListWidget(QWidget* parent)
	: QListWidget(parent)
{
	QAction* action;
	QFile knownTrackers;
	QMenu* submenu = m_menu.addMenu(tr("Add known"));

	if (openDataFile(&knownTrackers, "/data/bttrackers.txt"))
	{
		char buf[512];
		while (knownTrackers.readLine(buf, sizeof(buf)) > 0)
		{
			if (!buf[0])
				continue;
			char* p = strrchr(buf, '\n');
			if (p)
				*p = 0;
			submenu->addAction(buf, this, SLOT(addKnownItem()));
		}
	}

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

void ContextListWidget::addKnownItem()
{
	QAction* s = (QAction*) sender();
	QListWidgetItem* item = new QListWidgetItem(QIcon(":/fatrat/miscellaneous.png"), s->text(), this);
	QListWidget::addItem(item);
	item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
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
