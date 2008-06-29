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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
USA.
*/

#ifndef _QUEUEDLG_H
#define _QUEUEDLG_H
#include <QObject>
#include <QDialog>
#include <QPushButton>
#include "ui_QueueDlg.h"

class QueueDlg : public QDialog, Ui_QueueDlg
{
Q_OBJECT
public:
	QueueDlg(QWidget* parent=0) : QDialog(parent), m_nDownLimit(0), m_nUpLimit(0), m_nDownTransfersLimit(1), m_nUpTransfersLimit(1), m_bUpAsDown(false), m_bLimit(true)
	{
		setupUi(this);
		
		buttonBox->button(QDialogButtonBox::Cancel)->setAutoDefault(false);
		buttonBox->button(QDialogButtonBox::Cancel)->setDefault(false);
		buttonBox->button(QDialogButtonBox::Ok)->setAutoDefault(true);
		buttonBox->button(QDialogButtonBox::Ok)->setDefault(true);
		
		lineName->setFocus(Qt::OtherFocusReason);
		
		connect(groupLimitQueue, SIGNAL(clicked(bool)), this, SLOT(limitToggled(bool)));
		
		spinDown->setRange(0,INT_MAX);
		spinUp->setRange(0,INT_MAX);
		spinTransfersDown->setMinimum(1);
		spinTransfersUp->setMinimum(1);
	}
	int exec()
	{
		int r;
		
		lineName->setText(m_strName);
		
		spinDown->setValue(m_nDownLimit/1024);
		spinUp->setValue(m_nUpLimit/1024);
		spinTransfersDown->setValue(m_nDownTransfersLimit);
		spinTransfersUp->setValue(m_nUpTransfersLimit);
		checkUpAsDown->setChecked(m_bUpAsDown);
		groupLimitQueue->setChecked(m_bLimit);
		
		if((r = QDialog::exec()) == QDialog::Accepted)
		{
			m_strName = lineName->text();
			m_nDownLimit = spinDown->value()*1024;
			m_nUpLimit = spinUp->value()*1024;
			m_nDownTransfersLimit = spinTransfersDown->value();
			m_nUpTransfersLimit = spinTransfersUp->value();
			m_bUpAsDown = checkUpAsDown->isChecked();
			m_bLimit = groupLimitQueue->isChecked();
		}
		
		return r;
	}
public slots:
	virtual void accept()
	{
		if(!lineName->text().isEmpty())
			QDialog::accept();
	}
	void limitToggled(bool checked)
	{
		spinTransfersDown->setEnabled(checked);
		spinTransfersUp->setEnabled(checked);
		checkUpAsDown->setEnabled(checked);
	}
public:
	QString m_strName;
	int m_nDownLimit, m_nUpLimit, m_nDownTransfersLimit, m_nUpTransfersLimit;
	bool m_bUpAsDown, m_bLimit;
};

#endif
