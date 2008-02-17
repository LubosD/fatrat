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
	
	JabberService::instance()->applySettings();
}

