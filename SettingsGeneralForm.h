#ifndef _SETTINGSGENERALFORM_H
#define _SETTINGSGENERALFORM_H
#include "ui_SettingsGeneralForm.h"
#include "fatrat.h"
#include <QSettings>
#include <QDir>
#include <QMessageBox>
#include <QFileDialog>
#include <QSystemTrayIcon>
#include "WidgetHostChild.h"

extern QSettings* g_settings;

class SettingsGeneralForm : public QObject, Ui_SettingsGeneralForm, public WidgetHostChild
{
Q_OBJECT
public:
	SettingsGeneralForm(QWidget* me)
	{
		setupUi(me);
		
		connect(toolDestination, SIGNAL(pressed()), this, SLOT(browse()));
	}
	
	virtual void load()
	{
		lineDestination->setText( g_settings->value("defaultdir", getSettingsDefault("defaultdir")).toString() );
		lineFileExec->setText( g_settings->value("fileexec", getSettingsDefault("fileexec")).toString() );
		
		checkTrayIcon->setChecked( g_settings->value("trayicon", getSettingsDefault("trayicon")).toBool() );
		checkHideMinimize->setChecked( g_settings->value("hideminimize", getSettingsDefault("hideminimize")).toBool() );
		checkHideClose->setChecked( g_settings->value("hideclose", getSettingsDefault("hideclose")).toBool() );
		
		//spinSpeedThreshold->setValue( g_settings->value("speedthreshold", getSettingsDefault("speedthreshold")).toInt() );
		spinRetry->setValue( g_settings->value("retrycount", getSettingsDefault("retrycount")).toInt() );
		checkRetryWorking->setChecked( g_settings->value("retryworking", getSettingsDefault("retryworking")).toBool() );
		checkPopup->setChecked( g_settings->value("showpopup", getSettingsDefault("showpopup")).toBool() );
		spinPopup->setValue( g_settings->value("popuptime", getSettingsDefault("popuptime")).toInt() );
		checkEmail->setChecked( g_settings->value("sendemail", getSettingsDefault("sendemail")).toBool() );
		lineSmtp->setText( g_settings->value("smtpserver", getSettingsDefault("smtpserver")).toString() );
		lineSender->setText( g_settings->value("emailsender", getSettingsDefault("emailsender")).toString() );
		lineRecipient->setText( g_settings->value("emailrcpt", getSettingsDefault("emailrcpt")).toString() );
		spinGraphMinutes->setValue( g_settings->value("graphminutes", getSettingsDefault("graphminutes")).toInt() );
		
		checkPopup->setEnabled(QSystemTrayIcon::supportsMessages());
		checkAutoRemove->setChecked( g_settings->value("autoremove", getSettingsDefault("autoremove")).toBool() );
	}
	
	virtual bool accept()
	{
		if(lineDestination->text().isEmpty())
			return false;
		else
		{
			QDir dir(lineDestination->text());
			if(!dir.isReadable())
			{
				QMessageBox::critical(0, tr("Error"), tr("The specified directory is inaccessible."));
				return false;
			}
		}
		
		if(!lineSender->text().contains('@') || !lineRecipient->text().contains('@'))
		{
			QMessageBox::critical(0, tr("Error"), tr("The e-mail address is incorrect."));
			return false;
		}
		
		return true;
	}
	virtual void accepted()
	{
		g_settings->setValue("defaultdir", lineDestination->text());
		g_settings->setValue("execfile", lineFileExec->text());
		
		g_settings->setValue("trayicon", checkTrayIcon->isChecked());
		g_settings->setValue("hideminimize", checkHideMinimize->isChecked());
		g_settings->setValue("hideclose", checkHideClose->isChecked());
		
		//g_settings->setValue("speedthreshold", spinSpeedThreshold->value());
		//g_settings->setValue("distributenotactive", checkDistributeNotActive->isChecked());
		g_settings->setValue("retrycount", spinRetry->value());
		g_settings->setValue("retryworking", checkRetryWorking->isChecked());
		g_settings->setValue("showpopup", checkPopup->isChecked());
		g_settings->setValue("popuptime", spinPopup->value());
		g_settings->setValue("sendemail", checkEmail->isChecked());
		g_settings->setValue("smtpserver", lineSmtp->text());
		g_settings->setValue("emailsender", lineSender->text());
		g_settings->setValue("emailrcpt", lineRecipient->text());
		g_settings->setValue("graphminutes", spinGraphMinutes->value());
		g_settings->setValue("autoremove", checkAutoRemove->isChecked());
	}
public slots:
	void browse()
	{
		QString dir = QFileDialog::getExistingDirectory(0, tr("Choose directory"), lineDestination->text());
		if(!dir.isNull())
			lineDestination->setText(dir);
	}
};

#endif
