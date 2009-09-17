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

#include "HttpFtpSettings.h"
#include "UserAuthDlg.h"
#include "Settings.h"
#include <QMessageBox>

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
	
	checkForbidIPv6->setChecked(getSettingsValue("httpftp/forbidipv6").toBool());
	
	// LOAD PROXYS
	m_listProxy = Proxy::loadProxys();
	m_defaultProxy = getSettingsValue("httpftp/defaultproxy").toString();
	
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
	foreach(Auth a, m_listAuth)
		listAuths->addItem(a.strRegExp);
}

void HttpFtpSettings::accepted()
{
	int index = comboDefaultProxy->currentIndex();
	if(!index)
		m_defaultProxy = QUuid();
	else
		m_defaultProxy = m_listProxy[index-1].uuid;
	setSettingsValue("httpftp/defaultproxy", m_defaultProxy.toString());
	
	Auth::saveAuths(m_listAuth);
	setSettingsValue("httpftp/forbidipv6", checkForbidIPv6->isChecked());
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
