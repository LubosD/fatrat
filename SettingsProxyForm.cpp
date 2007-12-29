#include "SettingsProxyForm.h"
#include "ProxyDlg.h"
#include <QSettings>
#include <QMessageBox>
#include <QtDebug>

extern QSettings* g_settings;

SettingsProxyForm::SettingsProxyForm(QWidget* w)
{
	setupUi(w);
	
	connect(pushAdd, SIGNAL(clicked()), this, SLOT(proxyAdd()));
	connect(pushEdit, SIGNAL(clicked()), this, SLOT(proxyEdit()));
	connect(pushDelete, SIGNAL(clicked()), this, SLOT(proxyDelete()));
}

void SettingsProxyForm::load()
{
	m_listProxy = Proxy::loadProxys();
	
	listProxys->clear();
	foreach(Proxy p,m_listProxy)
		listProxys->addItem(p.toString());
}

void SettingsProxyForm::accepted()
{
	g_settings->beginWriteArray("httpftp/proxys");
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
}

void SettingsProxyForm::proxyAdd()
{
	ProxyDlg dlg(pushAdd->parentWidget());
	
	if(dlg.exec() == QDialog::Accepted)
	{
		dlg.m_data.uuid = QUuid::createUuid();
		m_listProxy << dlg.m_data;
		listProxys->addItem(dlg.m_data.toString());
	}
}

void SettingsProxyForm::proxyEdit()
{
	int index = listProxys->currentRow();
	if(index < 0)
		return;
	
	ProxyDlg dlg(pushAdd->parentWidget());
	dlg.m_data = m_listProxy[index];
	
	if(dlg.exec() == QDialog::Accepted)
	{
		m_listProxy[index] = dlg.m_data;
		listProxys->currentItem()->setText(dlg.m_data.toString());
	}
}

void SettingsProxyForm::proxyDelete()
{
	int index = listProxys->currentRow();
	if(index < 0)
		return;
	
	if(QMessageBox::warning(pushAdd->parentWidget(), "FatRat", tr("Do you really want to delete the selected proxy?"),
	   QMessageBox::Yes|QMessageBox::No) == QMessageBox::Yes)
	{
		delete listProxys->takeItem(index);
		m_listProxy.removeAt(index);
	}
}
