#include "SettingsQueueForm.h"
#include <QSettings>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include "fatrat.h"

extern QSettings* g_settings;

SettingsQueueForm::SettingsQueueForm(QWidget* w, QObject* parent) : QObject(parent)
{
	setupUi(w);
}

void SettingsQueueForm::load()
{
	spinRetry->setValue( g_settings->value("retrycount", getSettingsDefault("retrycount")).toInt() );
	checkRetryWorking->setChecked( g_settings->value("retryworking", getSettingsDefault("retryworking")).toBool() );
	checkPopup->setChecked( g_settings->value("showpopup", getSettingsDefault("showpopup")).toBool() );
	spinPopup->setValue( g_settings->value("popuptime", getSettingsDefault("popuptime")).toInt() );
	checkEmail->setChecked( g_settings->value("sendemail", getSettingsDefault("sendemail")).toBool() );
	lineSmtp->setText( g_settings->value("smtpserver", getSettingsDefault("smtpserver")).toString() );
	lineSender->setText( g_settings->value("emailsender", getSettingsDefault("emailsender")).toString() );
	lineRecipient->setText( g_settings->value("emailrcpt", getSettingsDefault("emailrcpt")).toString() );
	
	checkPopup->setEnabled(QSystemTrayIcon::supportsMessages());
	checkAutoRemove->setChecked( g_settings->value("autoremove", getSettingsDefault("autoremove")).toBool() );
}

void SettingsQueueForm::accepted()
{
	g_settings->setValue("retrycount", spinRetry->value());
	g_settings->setValue("retryworking", checkRetryWorking->isChecked());
	g_settings->setValue("showpopup", checkPopup->isChecked());
	g_settings->setValue("popuptime", spinPopup->value());
	g_settings->setValue("sendemail", checkEmail->isChecked());
	g_settings->setValue("smtpserver", lineSmtp->text());
	g_settings->setValue("emailsender", lineSender->text());
	g_settings->setValue("emailrcpt", lineRecipient->text());
	g_settings->setValue("autoremove", checkAutoRemove->isChecked());
}

bool SettingsQueueForm::accept()
{
	if(!lineSender->text().contains('@') || !lineRecipient->text().contains('@'))
	{
		QMessageBox::critical(0, tr("Error"), tr("The e-mail address is incorrect."));
		return false;
	}
	return true;
}
