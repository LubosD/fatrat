#include "HttpFtpSettings.h"
#include "ProxyDlg.h"
#include "UserAuthDlg.h"
#include <QSettings>
#include <QMessageBox>

extern QSettings* g_settings;

HttpFtpSettings::HttpFtpSettings(QWidget* w)
{
	setupUi(w);
	
	connect(pushAdd, SIGNAL(clicked()), this, SLOT(proxyAdd()));
	connect(pushEdit, SIGNAL(clicked()), this, SLOT(proxyEdit()));
	connect(pushDelete, SIGNAL(clicked()), this, SLOT(proxyDelete()));
	connect(pushSetDefault, SIGNAL(clicked()), this, SLOT(proxySetDefault()));
	connect(pushCancelDefault, SIGNAL(clicked()), this, SLOT(proxyCancelDefault()));
	
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
	
	foreach(Proxy p,m_listProxy)
	{
		listProxys->addItem( composeName(p) );
		if(p.uuid == m_defaultProxy)
		{
			QListWidgetItem* it = listProxys->item(listProxys->count()-1);
		
			QFont f = it->font();
			f.setBold(true);
			it->setFont(f);
			
			bFound = true;
		}
	}
	
	if(!bFound)
		m_defaultProxy = QUuid();
	
	// LOAD AUTHs
	m_listAuth = Auth::loadAuths();
	
	foreach(Auth a,m_listAuth)
	{
		listAuths->addItem(a.strRegExp);
	}
}

void HttpFtpSettings::accepted()
{
	g_settings->beginGroup("httpftp");
	
	g_settings->beginWriteArray("proxys");
	for(int i=0;i<m_listProxy.size();i++)
	{
		Proxy& p = m_listProxy[i];
		
		g_settings->setArrayIndex(i);
		g_settings->setValue("type", int(p.nType));
		g_settings->setValue("name", p.strName);
		g_settings->setValue("ip", p.strIP);
		g_settings->setValue("port", p.nPort);
		g_settings->setValue("user", p.strUser);
		g_settings->setValue("password", p.strPassword);
		g_settings->setValue("uuid", p.uuid.toString());
	}
	g_settings->endArray();
	
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

void HttpFtpSettings::proxyAdd()
{
	ProxyDlg dlg(pushAdd->parentWidget());
	
	if(dlg.exec() == QDialog::Accepted)
	{
		dlg.m_data.uuid = QUuid::createUuid();
		m_listProxy << dlg.m_data;
		listProxys->addItem( composeName(dlg.m_data) );
	}
}

void HttpFtpSettings::proxyEdit()
{
	int index = listProxys->currentRow();
	if(index < 0)
		return;
	
	ProxyDlg dlg(pushAdd->parentWidget());
	dlg.m_data = m_listProxy[index];
	
	if(dlg.exec() == QDialog::Accepted)
	{
		m_listProxy[index] = dlg.m_data;
		listProxys->currentItem()->setText(composeName( dlg.m_data) );
	}
}

void HttpFtpSettings::proxyDelete()
{
	int index = listProxys->currentRow();
	if(index < 0)
		return;
	
	if(QMessageBox::warning(pushAdd->parentWidget(), tr("Delete proxy"), tr("Do you really want to delete the selected proxy?"),
	   QMessageBox::Yes|QMessageBox::No) == QMessageBox::Yes)
	{
		if(m_defaultProxy == m_listProxy[index].uuid)
			m_defaultProxy = QUuid();
		
		delete listProxys->takeItem(index);
		m_listProxy.removeAt(index);
	}
}

void HttpFtpSettings::proxySetDefault()
{
	int index = listProxys->currentRow();
	if(index < 0)
		return;
	
	QListWidgetItem* it;
	
	if(!m_defaultProxy.isNull())
	{
		for(int i=0;i<m_listProxy.size();i++)
		{
			if(m_listProxy[i].uuid == m_defaultProxy)
			{
				it = listProxys->item(i);
		
				QFont f = it->font();
				f.setBold(false);
				it->setFont(f);
				break;
			}
		}
	}
	
	it = listProxys->item(index);
	QFont f = it->font();
	
	f.setBold(true);
	it->setFont(f);
	
	m_defaultProxy = m_listProxy[index].uuid;
}

void HttpFtpSettings::proxyCancelDefault()
{
	for(int i=0;i<m_listProxy.size();i++)
	{
		if(m_listProxy[i].uuid == m_defaultProxy)
		{
			QListWidgetItem* it = listProxys->item(i);
	
			QFont f = it->font();
			f.setBold(false);
			it->setFont(f);
			break;
		}
	}
	m_defaultProxy = QUuid();
}

QString HttpFtpSettings::composeName(const Proxy& p)
{
	return QString("%1 (%2)").arg(p.strName).arg( (p.nType==0) ? "HTTP" : "SOCKS 5");
}

void HttpFtpSettings::authAdd()
{
	UserAuthDlg dlg(true, pushAdd->parentWidget());
	
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
	
	UserAuthDlg dlg(true, pushAdd->parentWidget());
	dlg.m_auth = m_listAuth[index];
	
	if(dlg.exec() == QDialog::Accepted)
	{
		m_listAuth[index] = dlg.m_auth;
		listAuths->currentItem()->setText(dlg.m_auth.strRegExp);
	}
}

void HttpFtpSettings::authDelete()
{
	int index = listProxys->currentRow();
	if(index < 0)
		return;
	
	if(QMessageBox::warning(pushAdd->parentWidget(), tr("Delete user credentials"), tr("Do you really want to delete the selected user credentials?"),
	   QMessageBox::Yes|QMessageBox::No) == QMessageBox::Yes)
	{
		delete listAuths->takeItem(index);
		m_listAuth.removeAt(index);
	}
}
