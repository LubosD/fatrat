#ifndef _SETTINGSGENERALFORM_H
#define _SETTINGSGENERALFORM_H
#include "ui_SettingsGeneralForm.h"
#include <QSettings>
#include <QDir>
#include <QMessageBox>
#include <QFileDialog>
#include <QSystemTrayIcon>
#include "WidgetHostChild.h"

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
		QSettings settings;
		
		lineDestination->setText( settings.value("defaultdir", QDir::homePath()).toString() );
		
		checkTrayIcon->setChecked( settings.value("trayicon", true).toBool() );
		checkHideMinimize->setChecked( settings.value("hideminimize", false).toBool() );
		checkHideClose->setChecked( settings.value("hideclose", true).toBool() );
		
		spinSpeedThreshold->setValue( settings.value("speedthreshold", int(0)).toInt() );
		checkPopup->setChecked( settings.value("showpopup", true).toBool() );
		spinPopup->setValue( settings.value("popuptime", int(4)).toInt() );
		checkEmail->setChecked( settings.value("sendemail", false).toBool() );
		lineSmtp->setText( settings.value("smtpserver", "localhost").toString() );
		lineSender->setText( settings.value("emailsender", "root@localhost").toString() );
		lineRecipient->setText( settings.value("emailrcpt", "root@localhost").toString() );
		spinGraphMinutes->setValue( settings.value("graphminutes", int(5)).toInt() );
		
		checkPopup->setEnabled(QSystemTrayIcon::supportsMessages());
		checkAutoRemove->setChecked( settings.value("autoremove", false).toBool() );
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
		QSettings settings;
		
		settings.setValue("defaultdir", lineDestination->text());
		
		settings.setValue("trayicon", checkTrayIcon->isChecked());
		settings.setValue("hideminimize", checkHideMinimize->isChecked());
		settings.setValue("hideclose", checkHideClose->isChecked());
		
		settings.setValue("speedthreshold", spinSpeedThreshold->value());
		//settings.setValue("distributenotactive", checkDistributeNotActive->isChecked());
		settings.setValue("showpopup", checkPopup->isChecked());
		settings.setValue("popuptime", spinPopup->value());
		settings.setValue("sendemail", checkEmail->isChecked());
		settings.setValue("smtpserver", lineSmtp->text());
		settings.setValue("emailsender", lineSender->text());
		settings.setValue("emailrcpt", lineRecipient->text());
		settings.setValue("graphminutes", spinGraphMinutes->value());
		settings.setValue("autoremove", checkAutoRemove->isChecked());
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
