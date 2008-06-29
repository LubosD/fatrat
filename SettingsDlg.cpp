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

#include "config.h"

#include "SettingsDlg.h"
#include "SettingsGeneralForm.h"
#include "SettingsDropBoxForm.h"
#include "SettingsNetworkForm.h"
#include "SettingsQueueForm.h"
#include "MainWindow.h"

#ifdef WITH_JABBER
#	include "remote/SettingsJabberForm.h"
#endif
//#ifdef WITH_BITTORRENT
#	include "rss/SettingsRssForm.h"
//#endif

#include <QSettings>

extern QSettings* g_settings;

SettingsDlg::SettingsDlg(QWidget* parent) : QDialog(parent)
{
	setupUi(this);
	
	QWidget* w = new QWidget(stackedWidget);
	
	m_children << (WidgetHostChild*)(new SettingsGeneralForm(w, this));
	listWidget->addItem( new QListWidgetItem(QIcon(":/fatrat/fatrat.png"), tr("Main"), listWidget) );
	stackedWidget->addWidget(w);
	
	w = new QWidget(stackedWidget);
	m_children << (WidgetHostChild*)(new SettingsQueueForm(w, this));
	listWidget->addItem( new QListWidgetItem(QIcon(":/fatrat/queue.png"), tr("Queue"), listWidget) );
	stackedWidget->addWidget(w);
	
	w = new QWidget(stackedWidget);
	m_children << (WidgetHostChild*)(new SettingsDropBoxForm(w, this));
	listWidget->addItem( new QListWidgetItem(QIcon(":/fatrat/miscellaneous.png"), tr("Drop-box"), listWidget) );
	stackedWidget->addWidget(w);
	
	w = new QWidget(stackedWidget);
	m_children << (WidgetHostChild*)(new SettingsNetworkForm(w, this));
	listWidget->addItem( new QListWidgetItem(QIcon(":/menu/network.png"), tr("Network"), listWidget) );
	stackedWidget->addWidget(w);
	
	listWidget->setCurrentRow(0);
	
	fillEngines( Transfer::engines(Transfer::Download) );
	fillEngines( Transfer::engines(Transfer::Upload) );
	
//#ifdef WITH_BITTORRENT
	w = new QWidget(stackedWidget);
	m_children << (WidgetHostChild*)(new SettingsRssForm(w, this));
	listWidget->addItem( new QListWidgetItem(QIcon(":/fatrat/rss.png"), tr("RSS"), listWidget) );
	stackedWidget->addWidget(w);
//#endif
	
#ifdef WITH_JABBER
	w = new QWidget(stackedWidget);
	m_children << (WidgetHostChild*)(new SettingsJabberForm(w, this));
	listWidget->addItem( new QListWidgetItem(QIcon(":/fatrat/jabber.png"), tr("Jabber"), listWidget) );
	stackedWidget->addWidget(w);
#endif
	
	connect(buttonBox, SIGNAL(clicked(QAbstractButton*)), this, SLOT(buttonClicked(QAbstractButton*)));
}

SettingsDlg::~SettingsDlg()
{
	qDeleteAll(m_children);
}

void SettingsDlg::accept()
{
	bool accepted = true, acc;
	
	foreach(WidgetHostChild* w,m_children)
	{
		acc = w->accept();
		accepted = accepted && acc;
		
		if(!accepted)
			break;
	}
	
	if(accepted)
	{
		foreach(WidgetHostChild* w,m_children)
			w->accepted();
		
		QDialog::accept();
		
		g_settings->sync();
		static_cast<MainWindow*>(getMainWindow())->reconfigure();
	}
}

int SettingsDlg::exec()
{
	foreach(WidgetHostChild* w,m_children)
		w->load();
	
	return QDialog::exec();
}

void SettingsDlg::buttonClicked(QAbstractButton* btn)
{
	if(buttonBox->buttonRole(btn) == QDialogButtonBox::ApplyRole)
	{
		foreach(WidgetHostChild* w,m_children)
		{
			bool acc = w->accept();
			
			if(!acc)
				return;
		}
		
		foreach(WidgetHostChild* w,m_children)
			w->accepted();
		foreach(WidgetHostChild* w,m_children)
			w->load();
		
		static_cast<MainWindow*>(getMainWindow())->reconfigure();
	}
}

void SettingsDlg::fillEngines(const EngineEntry* engines)
{
	for(int i=0;engines[i].shortName;i++)
	{
		if(engines[i].lpfnSettings)
		{
			QIcon icon;
			QWidget* w = new QWidget(stackedWidget);
			WidgetHostChild* c = engines[i].lpfnSettings(w,icon);
			
			if(c == 0)
			{
				// omfg
				delete w;
				continue;
			}
			else
			{
				stackedWidget->addWidget(w);
				listWidget->addItem( new QListWidgetItem(icon, w->windowTitle(), listWidget) );
				m_children << c;
			}
		}
	}
}
