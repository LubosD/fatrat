#include "SettingsNetworkForm.h"
#include "ProxyDlg.h"
#include <QSettings>
#include <QMessageBox>

extern QSettings* g_settings;

SettingsNetworkForm::SettingsNetworkForm(QWidget* w, QObject* parent) : QObject(parent)
{
	setupUi(w);
	
	connect(pushAdd, SIGNAL(clicked()), this, SLOT(proxyAdd()));
	connect(pushEdit, SIGNAL(clicked()), this, SLOT(proxyEdit()));
	connect(pushDelete, SIGNAL(clicked()), this, SLOT(proxyDelete()));
}

void SettingsNetworkForm::load()
{
	spinDown->setValue( g_settings->value("network/speed_down", getSettingsDefault("network/speed_down")).toInt() / 1024 );
	spinUp->setValue( g_settings->value("network/speed_up", getSettingsDefault("network/speed_up")).toInt() / 1024 );
	
	m_listProxy = Proxy::loadProxys();
	
	listProxys->clear();
	foreach(Proxy p,m_listProxy)
		listProxys->addItem(p.toString());
}

void SettingsNetworkForm::accepted()
{
	g_settings->setValue("network/speed_down", spinDown->value() * 1024);
	g_settings->setValue("network/speed_up", spinUp->value() * 1024);
	
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

void SettingsNetworkForm::proxyAdd()
{
	ProxyDlg dlg(pushAdd->parentWidget());
	
	if(dlg.exec() == QDialog::Accepted)
	{
		dlg.m_data.uuid = QUuid::createUuid();
		m_listProxy << dlg.m_data;
		listProxys->addItem(dlg.m_data.toString());
	}
}

void SettingsNetworkForm::proxyEdit()
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

void SettingsNetworkForm::proxyDelete()
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

