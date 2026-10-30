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

#include "QueueDlg.h"
#include "Queue.h"
#include <QFileDialog>
#include <QPushButton>
#include <QMessageBox>
#include <QReadLocker>
#include <QDir>
#include "watch/WatchFolderQueues.h"
#include "watch/WatchFolderStatus.h"

QueueDlg::QueueDlg(QWidget* parent)
	: QDialog(parent), m_nDownLimit(0), m_nUpLimit(0), m_nDownTransfersLimit(1), m_nUpTransfersLimit(1), m_bUpAsDown(false), m_bLimit(true)
{
	setupUi(this);
	
	buttonBox->button(QDialogButtonBox::Cancel)->setAutoDefault(false);
	buttonBox->button(QDialogButtonBox::Cancel)->setDefault(false);
	buttonBox->button(QDialogButtonBox::Ok)->setAutoDefault(true);
	buttonBox->button(QDialogButtonBox::Ok)->setDefault(true);
	
	lineName->setFocus(Qt::OtherFocusReason);
	
	connect(groupLimitQueue, SIGNAL(clicked(bool)), this, SLOT(limitToggled(bool)));
	connect(toolDestination, SIGNAL(pressed()), this, SLOT(browse()));
	connect(toolMoveCompleted, SIGNAL(clicked()), this, SLOT(browseMove()));
	connect(toolWatch, SIGNAL(clicked()), this, SLOT(browseWatch()));
	
	spinDown->setRange(0,INT_MAX);
	spinUp->setRange(0,INT_MAX);
	spinTransfersDown->setMinimum(1);
	spinTransfersUp->setMinimum(1);
	
	m_strDefaultDirectory = Queue::defaultDownloadDirectory();
}

int QueueDlg::exec()
{
	int r;
	
	lineName->setText(m_strName);
	
	spinDown->setValue(m_nDownLimit/1024);
	spinUp->setValue(m_nUpLimit/1024);
	spinTransfersDown->setValue(m_nDownTransfersLimit);
	spinTransfersUp->setValue(m_nUpTransfersLimit);
	checkUpAsDown->setChecked(m_bUpAsDown);
	groupLimitQueue->setChecked(m_bLimit);
	lineDestination->setText(m_strDefaultDirectory);
	
	if(m_strMoveDirectory.isEmpty())
		groupMoveCompleted->setChecked(false);
	else
	{
		groupMoveCompleted->setChecked(true);
		lineMoveCompleted->setText(m_strMoveDirectory);
	}
	
	groupWatch->setChecked(!m_strWatchDirectory.isEmpty());
	lineWatch->setText(m_strWatchDirectory);
	labelWatchStatus->setText(WatchFolderStatus::describe(m_strWatchStatus));
	labelWatchStatus->setVisible(!labelWatchStatus->text().isEmpty());
	
	if((r = QDialog::exec()) == QDialog::Accepted)
	{
		m_strName = lineName->text();
		m_nDownLimit = spinDown->value()*1024;
		m_nUpLimit = spinUp->value()*1024;
		m_nDownTransfersLimit = spinTransfersDown->value();
		m_nUpTransfersLimit = spinTransfersUp->value();
		m_bUpAsDown = checkUpAsDown->isChecked();
		m_bLimit = groupLimitQueue->isChecked();
		m_strDefaultDirectory = lineDestination->text();
		
		if(groupMoveCompleted->isChecked())
			m_strMoveDirectory = lineMoveCompleted->text();
		else
			m_strMoveDirectory.clear();
		
		m_strWatchDirectory = groupWatch->isChecked() ? lineWatch->text().trimmed() : QString();
	}
	
	return r;
}

void QueueDlg::accept()
{
	if(lineName->text().isEmpty())
		return;

	const QString watch = lineWatch->text().trimmed();
	if(groupWatch->isChecked() && !watch.isEmpty())
	{
		if(!QDir::isAbsolutePath(watch))
		{
			QMessageBox::warning(this, tr("Watch folder"), tr("The watch folder must be an absolute path."));
			return;
		}

		QString owner;
		{
			QReadLocker l(&g_queuesLock);
			const QString uuid = WatchFolderStatus::owner(WatchFolderQueues::entries(), watch, m_strQueueUuid);
			for(Queue* q : g_queues)
			{
				if(q->uuid() == uuid)
					owner = q->name();
			}
		}
		if(!owner.isEmpty())
		{
			QMessageBox::warning(this, tr("Watch folder"), tr("The queue \"%1\" already watches this folder.").arg(owner));
			return;
		}
	}
	QDialog::accept();
}

void QueueDlg::limitToggled(bool checked)
{
	spinTransfersDown->setEnabled(checked);
	spinTransfersUp->setEnabled(checked);
	checkUpAsDown->setEnabled(checked);
}
void QueueDlg::browse()
{
	QString dir = QFileDialog::getExistingDirectory(0, tr("Choose directory"), lineDestination->text());
	if(!dir.isNull())
		lineDestination->setText(dir);
}

void QueueDlg::browseMove()
{
	QString dir = QFileDialog::getExistingDirectory(0, tr("Choose directory"), lineMoveCompleted->text());
	if(!dir.isNull())
		lineMoveCompleted->setText(dir);
}

void QueueDlg::browseWatch()
{
	QString dir = QFileDialog::getExistingDirectory(0, tr("Choose directory"), lineWatch->text());
	if(!dir.isNull())
		lineWatch->setText(dir);
}
