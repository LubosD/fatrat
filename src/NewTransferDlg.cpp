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

#include "NewTransferDlg.h"
#include "Settings.h"
#include "Queue.h"
#include "UserAuthDlg.h"
#include <QFileDialog>
#include <QSettings>
#include <QReadWriteLock>
#include <QMenu>
#include <QClipboard>
#include <QtDebug>

extern QSettings* g_settings;
extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;
extern QVector<EngineEntry> g_enginesDownload;
extern QVector<EngineEntry> g_enginesUpload;

NewTransferDlg::NewTransferDlg(QWidget* parent)
: QDialog(parent), m_bDetails(false), m_bPaused(false), m_nDownLimit(0),
		m_nUpLimit(0), m_nClass(-1), m_mode(Transfer::Download),
		m_nQueue(0)
{
	setupUi(this);

	textURIs->setFocus(Qt::OtherFocusReason);
	spinDown->setRange(0,INT_MAX);
	spinUp->setRange(0,INT_MAX);
	
	connect(toolBrowse, SIGNAL(pressed()), this, SLOT(browse()));
	connect(radioDownload, SIGNAL(clicked()), this, SLOT(switchMode()));
	connect(radioUpload, SIGNAL(clicked()), this, SLOT(switchMode()));
	connect(pushAddFiles, SIGNAL(clicked()), this, SLOT(browse2()));
	connect(toolAuth, SIGNAL(clicked()), this, SLOT(authData()));
	connect(toolAuth2, SIGNAL(clicked()), this, SLOT(authData()));
	
	comboClass->addItem(tr("Auto detect"));
	
	for(int i=0;i<g_enginesDownload.size();i++)
		comboClass->addItem(g_enginesDownload[i].longName);
	
	comboClass2->addItem(tr("Auto detect"));
	
	for(int i=0;i<g_enginesUpload.size();i++)
		comboClass2->addItem(g_enginesUpload[i].longName);
	
	QMenu* menu = new QMenu(toolAddSpecial);
	QAction* act;
	
	act = menu->addAction(tr("Add local files..."));
	connect(act, SIGNAL(triggered()), this, SLOT(browse2()));
	
	act = menu->addAction(tr("Add contents of a text file..."));
	connect(act, SIGNAL(triggered()), this, SLOT(addTextFile()));
	
	act = menu->addAction(tr("Add from clipboard"));
	connect(act, SIGNAL(triggered()), this, SLOT(addClipboard()));
	
	toolAddSpecial->setMenu(menu);
}

int NewTransferDlg::exec()
{
	int result;
	
	load();
	result = QDialog::exec();
	
	if(result == QDialog::Accepted)
		accepted();
	
	return result;
}

/*
void NewTransferDlg::accept()
{
	
	if(radioDownload->isChecked())
	{
		if((!m_bNewTransfer || !textURIs->toPlainText().isEmpty()) && !comboDestination->currentText().isEmpty())
		{
			QDir dir(comboDestination->currentText());
			if(dir.isReadable())
			{
				QDialog::accept();
				return;
			}
		}
	}
	else
		QDialog::accept();
	
}
*/

void NewTransferDlg::accepted()
{
	m_mode = radioDownload->isChecked() ? Transfer::Download : Transfer::Upload;
	if(m_mode == Transfer::Download)
	{
		m_strURIs = textURIs->toPlainText();
		m_strDestination = comboDestination->currentText();
		m_nClass = comboClass->currentIndex()-1;
		
		if(!m_lastDirs.contains(m_strDestination) && !m_strDestination.isEmpty())
		{
			if(m_lastDirs.size() >= 5)
				m_lastDirs.removeFirst();
			m_lastDirs.append(m_strDestination);
			g_settings->setValue("lastdirs", m_lastDirs);
			g_settings->sync();
		}
	}
	else
	{
		m_strURIs = textFiles->toPlainText();
		m_strDestination = comboDestination2->currentText();
		m_nClass = comboClass2->currentIndex()-1;
	}
	
	m_bDetails = checkDetails->isChecked();
	m_bPaused = checkPaused->isChecked();
	m_nDownLimit = spinDown->value()*1024;
	m_nUpLimit = spinUp->value()*1024;
	
	m_nQueue = comboQueue->currentIndex();
}

QString NewTransferDlg::getDestination() const
{
	if(radioDownload->isChecked())
		return comboDestination->currentText();
	else
		return comboDestination2->currentText();
}

void NewTransferDlg::setDestination(QString p)
{
	if(radioDownload->isChecked())
		comboDestination->setEditText(p);
	else
		comboDestination2->setEditText(p);
}

void NewTransferDlg::addLinks(QString links)
{
	m_mode = radioDownload->isChecked() ? Transfer::Download : Transfer::Upload;
	if(m_mode == Transfer::Download)
		textURIs->append(links);
	else
		textFiles->append(links);
}

void NewTransferDlg::load()
{
	/*if(!m_strClass.isEmpty())
	{
		if(m_lastDirs.size() >= 5)
			m_lastDirs.removeFirst();
		m_lastDirs.append(m_strDestination);
		g_settings->setValue("lastdirs", m_lastDirs);
	}*/
	
	{
		bool bFound = false;
		for(int i=0;i<g_enginesDownload.size();i++)
		{
			if(m_strClass == g_enginesDownload[i].shortName)
			{
				comboClass->setCurrentIndex(i+1);
				bFound = true;
				break;
			}
		}
		
		if(!bFound)
		{
			for(int i=0;i<g_enginesDownload.size();i++)
			{
				if(m_strClass == g_enginesDownload[i].shortName)
				{
					comboClass2->setCurrentIndex(i+1);
					break;
				}
			}
		}
	}
	{
		QReadLocker l(&g_queuesLock);
		
		for(int i=0;i<g_queues.size();i++)
		{
			comboQueue->addItem(g_queues[i]->name());
			comboQueue->setItemData(i, g_queues[i]->defaultDirectory());
		}
		
		if(g_queues.isEmpty())
			m_nQueue = -1;
		
		comboQueue->setCurrentIndex(m_nQueue);
		if(m_strDestination.isEmpty())
		{
			if(m_nQueue != -1)
				m_strDestination = g_queues[m_nQueue]->defaultDirectory();
			else
				m_strDestination = QDir::homePath();
		}
	}
	
	m_lastDirs = g_settings->value("lastdirs").toStringList();
	if(!m_lastDirs.contains(m_strDestination))
		comboDestination->addItem(m_strDestination);
	comboDestination->addItems(m_lastDirs);
	comboDestination->setEditText(m_strDestination);
	
	spinDown->setValue(m_nDownLimit/1024);
	spinUp->setValue(m_nUpLimit/1024);
	checkDetails->setChecked(m_bDetails);
	checkPaused->setChecked(m_bPaused);
	
	if(m_mode == Transfer::Upload)
	{
		stackedWidget->setCurrentIndex(1);
		radioUpload->setChecked(true);
		textFiles->setText(m_strURIs);
	}
	else
		textURIs->setText(m_strURIs);
	
	connect(comboQueue, SIGNAL(currentIndexChanged(int)), this, SLOT(queueChanged(int)));
}

void NewTransferDlg::queueChanged(int now)
{
	if(now == m_nQueue)
		return;
	
	if(getDestination() == comboQueue->itemData(m_nQueue).toString())
		setDestination(comboQueue->itemData(now).toString());
	m_nQueue = now;
}

void NewTransferDlg::addTextFile()
{
	QString filename = QFileDialog::getOpenFileName(this, "FatRat");
	if(!filename.isNull())
	{
		QFile file(filename);
		if(file.open(QIODevice::ReadOnly))
			textURIs->append(file.readAll());
	}
}

void NewTransferDlg::addClipboard()
{
	QClipboard* cb = QApplication::clipboard();
	if(radioUpload->isChecked())
		textFiles->append(cb->text());
	else
		textURIs->append(cb->text());
}

void NewTransferDlg::browse()
{
	QString dir = QFileDialog::getExistingDirectory(this, "FatRat", comboDestination->currentText());
	if(!dir.isNull())
		comboDestination->setEditText(dir);
}

void NewTransferDlg::browse2()
{
	QStringList files;
	
	files = QFileDialog::getOpenFileNames(this, "FatRat");
	
	if(radioUpload->isChecked())
		textFiles->append(files.join("\n"));
	else
		textURIs->append(files.join("\n"));
}

/*
void NewTransferDlg::textChanged()
{
	QStringList list;
	
	if(radioDownload->isChecked())
		list = textURIs->toPlainText().split(QRegExp('\n'), QString::SkipEmptyParts);
	else
		list = textFiles->toPlainText().split(QRegExp('\n'), QString::SkipEmptyParts);
	checkDetails->setEnabled(list.size() <= 1);
}
*/

void NewTransferDlg::switchMode()
{
	if(radioUpload->isChecked())
	{
		stackedWidget->setCurrentIndex(1);
		textFiles->setText( textURIs->toPlainText() );
	}
	else
	{
		stackedWidget->setCurrentIndex(0);
		textURIs->setText( textFiles->toPlainText() );
	}
}

void NewTransferDlg::authData()
{
	UserAuthDlg dlg(false, this);
	dlg.m_auth = m_auth;
	
	if(dlg.exec() == QDialog::Accepted)
		m_auth = dlg.m_auth;
}

