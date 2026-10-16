/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2011 Lubos Dolezel <lubos a dolezel.info>

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


#include "FtpUploadOptsForm.h"
#include "Proxy.h"
#include "Auth.h"
#include "Settings.h"

///////////////////////////////////////////

FtpUploadOptsForm::FtpUploadOptsForm(QWidget* me, CurlUpload* myobj)
	: m_upload(myobj)
{
	setupUi(me);
	
	if(myobj->m_strTarget.scheme() != "ftp")
	{
		//comboFtpMode->setDisabled(true);
		comboProxy->setDisabled(true);
	}
}

void FtpUploadOptsForm::load()
{
	QList<Proxy> listProxy = Proxy::loadProxys();
	QUrl temp, url;
	
	temp = url = m_upload->m_strTarget;
	temp.setUserInfo(QString());
	lineTarget->setText(temp.toString());
	lineUsername->setText(url.userName());
	linePassword->setText(url.password());
	
	comboFtpMode->addItems(QStringList(tr("Active mode")) << tr("Passive mode"));
	comboFtpMode->setCurrentIndex(int(m_upload->m_mode));
	
	comboProxy->addItem(tr("(none)", "No proxy"));
	comboProxy->setCurrentIndex(0);
	
	for(int i=0;i<listProxy.size();i++)
	{
		comboProxy->addItem(listProxy[i].strName);
		if(listProxy[i].uuid == m_upload->m_proxy)
			comboProxy->setCurrentIndex(i+1);
	}
	
	lineAddrBind->setText(m_upload->m_strBindAddress);
}

void FtpUploadOptsForm::accepted()
{
	QList<Proxy> listProxy = Proxy::loadProxys();
	QUrl url = lineTarget->text();
	
	url.setUserName(lineUsername->text());
	url.setPassword(linePassword->text());
	
	m_upload->m_strTarget = url.toString();
	int ix = comboProxy->currentIndex();
	m_upload->m_proxy = (!ix) ? QUuid() : listProxy[ix-1].uuid;
	
	m_upload->m_mode = FtpMode( comboFtpMode->currentIndex() );
	m_upload->m_strBindAddress = lineAddrBind->text();
}

bool FtpUploadOptsForm::accept()
{
	bool acc = false;
	
	acc |= lineTarget->text().startsWith("ftp://");
	acc |= lineTarget->text().startsWith("sftp://");
	
	return acc;
}

