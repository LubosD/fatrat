/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2009 Lubos Dolezel <lubos a dolezel.info>

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

#include "ScheduledActionDlg.h"
#include "Queue.h"
#include <QMessageBox>

ScheduledActionDlg::ScheduledActionDlg(QWidget* parent)
	: QDialog(parent)
{
	setupUi(this);
	
	connect(radioRepeated, SIGNAL(clicked()), this, SLOT(switchPages()));
	connect(radioOneTime, SIGNAL(clicked()), this, SLOT(switchPages()));

	comboAction->addItems(QStringList() << tr("Resume all") << tr("Stop all") << tr("Set speed limit"));
	connect(comboAction, SIGNAL(currentIndexChanged(int)), stackedArguments, SLOT(setCurrentIndex(int)));
}

void ScheduledActionDlg::load()
{
	g_queuesLock.lockForRead();
	for(int i=0;i<g_queues.size();i++)
	{
		comboQueue->addItem(g_queues[i]->name());
		comboQueue->setItemData(i, g_queues[i]->uuid());
		
		if(g_queues[i]->uuid() == m_action.queue.toString())
			comboQueue->setCurrentIndex(i);
	}
	g_queuesLock.unlock();
	
	lineName->setText(m_action.name);
	timeEdit->setTime(m_action.whenRepeated);
	dateTimeEdit->setDateTime(m_action.whenOneTime);
	radioRepeated->setChecked(m_action.repeated);
	radioOneTime->setChecked(!m_action.repeated);
	comboAction->setCurrentIndex(int(m_action.action));
	
	for(int i=0;i<7;i++)
		listWeekdays->item(i)->setCheckState(m_action.daysRepeated[i] ? Qt::Checked : Qt::Unchecked);

	if(m_action.action == ScheduledAction::ActionSetSpeedLimit)
	{
		QList<QVariant> sp = m_action.actionArgument.toList();
		if (sp.size() == 2)
		{
			lineDown->setText(QString::number(sp[0].toInt()/ 1024));
			lineUp->setText(QString::number(sp[1].toInt() / 1024));
		}
	}
	
	switchPages();
}

void ScheduledActionDlg::save()
{
	int index = comboQueue->currentIndex();
	if (index == -1)
		m_action.queue = QUuid();
	else
		m_action.queue = comboQueue->itemData(comboQueue->currentIndex()).toString();
	m_action.name = lineName->text();
	m_action.repeated = radioRepeated->isChecked();
	m_action.whenRepeated = timeEdit->time();
	m_action.whenOneTime = dateTimeEdit->dateTime();
	m_action.action = ScheduledAction::ActionType(comboAction->currentIndex());

	if(m_action.action == ScheduledAction::ActionSetSpeedLimit)
	{
		QList<QVariant> sp;
		sp << lineDown->text().toInt() * 1024;
		sp << lineUp->text().toInt() * 1024;
		m_action.actionArgument = sp;
	}
	
	for(int i=0;i<7;i++)
		m_action.daysRepeated[i] = listWeekdays->item(i)->checkState() == Qt::Checked;
}

void ScheduledActionDlg::accept()
{
	if(lineName->text().isEmpty())
	{
		QMessageBox::warning(this, "FatRat", tr("Enter the action name!"));
	}
	else
		QDialog::accept();
}

int ScheduledActionDlg::exec()
{
	load();
	
	int ret = QDialog::exec();
	if(ret == QDialog::Accepted)
		save();
	
	return ret;
}

void ScheduledActionDlg::switchPages()
{
	stackedWidget->setCurrentIndex(radioOneTime->isChecked() ? 1 : 0);
}

