#include "SettingsJabberForm.h"
#include "JabberService.h"
#include <QSettings>

extern QSettings* g_settings;

SettingsJabberForm::SettingsJabberForm(QWidget* w, QObject* parent)
	: QObject(parent)
{
	setupUi(w);
}

void SettingsJabberForm::load()
{
	checkEnable->setChecked(g_settings->value("jabber/enabled", getSettingsDefault("jabber/enabled")).toBool());
	lineJID->setText(g_settings->value("jabber/jid").toString());
	linePassword->setText(g_settings->value("jabber/password").toString());
	checkRestrictSelf->setChecked(g_settings->value("jabber/restrict_self", getSettingsDefault("jabber/restrict_self")).toBool());
	groupRestrictPassword->setChecked(g_settings->value("jabber/restrict_password_bool").toBool());
	lineRestrictPassword->setText(g_settings->value("jabber/restrict_password").toString());
	spinPriority->setValue(g_settings->value("jabber/priority", getSettingsDefault("jabber/priority")).toInt());
	
	QUuid uuid = g_settings->value("jabber/proxy").toString();
	m_listProxy = Proxy::loadProxys();
	
	comboProxy->addItem(tr("None", "No proxy"));
	for(int i=0;i<m_listProxy.size();i++)
	{
		comboProxy->addItem(m_listProxy[i].toString());
		if(uuid == m_listProxy[i].uuid)
			comboProxy->setCurrentIndex(i+1);
	}
}

void SettingsJabberForm::accepted()
{
	g_settings->setValue("jabber/enabled", checkEnable->isChecked());
	g_settings->setValue("jabber/jid", lineJID->text());
	g_settings->setValue("jabber/password", linePassword->text());
	g_settings->setValue("jabber/restrict_self", checkRestrictSelf->isChecked());
	g_settings->setValue("jabber/restrict_password_bool", groupRestrictPassword->isChecked());
	g_settings->setValue("jabber/restrict_password", lineRestrictPassword->text());
	g_settings->setValue("jabber/priority", spinPriority->value());
	
	int index = comboProxy->currentIndex() - 1;
	g_settings->setValue("jabber/proxy", (index >= 0) ? m_listProxy[index].uuid.toString() : "");
	
	JabberService::instance()->applySettings();
}

