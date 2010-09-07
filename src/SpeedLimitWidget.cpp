/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#include "SpeedLimitWidget.h"
#include "MainWindow.h"
#include "Queue.h"
#include "fatrat.h"
#include <QMenu>
#include <QLineEdit>
#include <QWidgetAction>

extern MainWindow* g_wndMain;

SpeedLimitWidget::SpeedLimitWidget(QWidget* parent)
: QWidget(parent)
{
	setupUi(this);
	m_timer.start(1000);
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(refresh()));
	
	labelUp->setUpload(true);
	labelUp2->setUpload(true);
}

void SpeedLimitWidget::refresh()
{
	Queue* q = g_wndMain->getCurrentQueue();
	if(q != 0)
	{
		int down,up;
		q->speedLimits(down,up);
		
		g_wndMain->doneQueue(q, true, false);
		
		if(down > 0)
			labelDown->setText(formatSize(down,true));
		else
			labelDown->setText(QString::fromUtf8("∞ kB/s"));
		
		if(up > 0)
			labelUp->setText(formatSize(up,true));
		else
			labelUp->setText(QString::fromUtf8("∞ kB/s"));
		
		labelDown->refresh(down);
		labelDown2->refresh(down);
		labelUp->refresh(up);
		labelUp2->refresh(up);
	}
}

void SpeedLimitWidget::mouseDoubleClickEvent(QMouseEvent*)
{
	g_wndMain->queueItemProperties();
}

void RightClickLabel::setLimit()
{
	Queue* q = g_wndMain->getCurrentQueue();
	if(q != 0)
	{
		QAction* action = (QAction*) sender();
		int speed = action->data().toInt() * 1024;
		int down, up;
		
		q->speedLimits(down,up);
		
		if(m_bUpload)
			up = speed;
		else
			down = speed;
		
		q->setSpeedLimits(down, up);
		
		g_wndMain->doneQueue(q);
	}
}

void RightClickLabel::customSpeedEntered()
{
	QLineEdit* line = static_cast<QLineEdit*>(sender());
	QString s = line->text();
	int speed = 0;

	if (!s.isEmpty())
		speed = s.toInt() * 1024;

	Queue* q = g_wndMain->getCurrentQueue();
	if(q != 0)
	{
		int down, up;

		q->speedLimits(down,up);

		if(m_bUpload)
			up = speed;
		else
			down = speed;

		q->setSpeedLimits(down, up);

		g_wndMain->doneQueue(q);
	}
}

void RightClickLabel::mousePressEvent(QMouseEvent* event)
{
	if(event->button() == Qt::RightButton)
	{
		QMenu menu;
		QAction* action;
		QLineEdit* lineEdit = new QLineEdit(&menu);
		QWidgetAction* wa = new QWidgetAction(&menu);
		int speed = (m_nSpeed) ? (m_nSpeed/1024) : 200;

		lineEdit->setText(QString::number(m_nSpeed/1024));
		//lineEdit->setInputMask("00000");
		wa->setDefaultWidget(lineEdit);
		connect(lineEdit, SIGNAL(returnPressed()), this, SLOT(customSpeedEntered()));
		connect(lineEdit, SIGNAL(returnPressed()), &menu, SLOT(close()));
		
		menu.setSeparatorsCollapsible(false);
		action = menu.addSeparator();
		action->setText(m_bUpload ? tr("Upload") : tr("Download"));
		menu.addAction(wa);
		menu.addSeparator();
		
		action = menu.addAction(QString::fromUtf8("∞ kB/s"));
		action->setData(0);
		connect(action, SIGNAL(triggered()), this, SLOT(setLimit()));
		menu.addSeparator();
		
		int step = speed/4;
		speed *= 2;
		
		for(int i=0;i<8 && speed;i++)
		{
			action = menu.addAction(formatSize(speed*1024, true));
			action->setData(speed);
			connect(action, SIGNAL(triggered()), this, SLOT(setLimit()));
			
			speed -= step;
		}
		
		menu.exec(QCursor::pos());
	}
}

void RightClickLabel::mouseDoubleClickEvent(QMouseEvent*)
{
	g_wndMain->queueItemProperties();
}
