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
*/

#include "HttpFtpSettings.h"
#include "UserAuthDlg.h"
#include <QSettings>
#include <QMessageBox>

extern QSettings* g_settings;

HttpFtpSettings::HttpFtpSettings(QWidget* w, QObject* parent)
	: QObject(parent)
{
	setupUi(w);
	
	connect(pushAuthAdd, SIGNAL(clicked()), this, SLOT(authAdd()));
	connect(pushAuthEdit, SIGNAL(clicked()), this, SLOT(authEdit()));
	connect(pushAuthDelete, SIGNAL(clicked()), this, SLOT(authDelete()));
}

void HttpFtpSettings::load()
{
	bool bFound = false;
	
	// LOAD PROXYS
	m_listProxy = Proxy::loadProxys();
	m_defaultProxy = g_settings->value("httpftp/defaultproxy").toString();
	
	comboDefaultProxy->clear();
	comboDefaultProxy->addItem(tr("None", "No proxy"));
	for(int i=0;i<m_listProxy.size();i++)
	{
		comboDefaultProxy->addItem(m_listProxy[i].toString());
		if(m_listProxy[i].uuid == m_defaultProxy)
		{
			comboDefaultProxy->setCurrentIndex(i+1);
			
			bFound = true;
		}
	}
	
	if(!bFound)
		m_defaultProxy = QUuid();
	
	// LOAD AUTHs
	m_listAuth = Auth::loadAuths();
	
	listAuths->clear();
	foreach(Auth a,m_listAuth)
	{
		listAuths->addItem(a.strRegExp);
	}
}

void HttpFtpSettings::accepted()
{
	g_settings->beginGroup("httpftp");
	
	int index = comboDefaultProxy->currentIndex();
	if(!index)
		m_defaultProxy = QUuid();
	else
		m_defaultProxy = m_listProxy[index-1].uuid;
	g_settings->setValue("defaultproxy", m_defaultProxy.toString());
	
	g_settings->beginWriteArray("auths");
	for(int i=0;i<m_listAuth.size();i++)
	{
		Auth& a = m_listAuth[i];
		
		g_settings->setArrayIndex(i);
		g_settings->setValue("regexp",a.strRegExp);
		g_settings->setValue("user",a.strUser);
		g_settings->setValue("password",a.strPassword);
	}
	g_settings->endArray();
	
	g_settings->endGroup();
}

void HttpFtpSettings::authAdd()
{
	UserAuthDlg dlg(true, pushAuthAdd->parentWidget());
	
	if(dlg.exec() == QDialog::Accepted)
	{
		m_listAuth << dlg.m_auth;
		listAuths->addItem(dlg.m_auth.strRegExp);
	}
}

void HttpFtpSettings::authEdit()
{
	int index = listAuths->currentRow();
	if(index < 0)
		return;
	
	UserAuthDlg dlg(true, pushAuthAdd->parentWidget());
	dlg.m_auth = m_listAuth[index];
	
	if(dlg.exec() == QDialog::Accepted)
	{
		m_listAuth[index] = dlg.m_auth;
		listAuths->currentItem()->setText(dlg.m_auth.strRegExp);
	}
}

void HttpFtpSettings::authDelete()
{
	int index = listAuths->currentRow();
	if(index < 0)
		return;
	
	if(QMessageBox::warning(pushAuthAdd->parentWidget(), tr("Delete user credentials"), tr("Do you really want to delete the selected user credentials?"),
	   QMessageBox::Yes|QMessageBox::No) == QMessageBox::Yes)
	{
		delete listAuths->takeItem(index);
		m_listAuth.removeAt(index);
	}
}
