#include "SettingsGeneralForm.h"
#include "fatrat.h"
#include <QSettings>
#include <QDir>
#include <QMessageBox>
#include <QFileDialog>

extern QSettings* g_settings;

SettingsGeneralForm::SettingsGeneralForm(QWidget* me, QObject* parent) : QObject(parent)
{
	setupUi(me);
	
	connect(toolDestination, SIGNAL(pressed()), this, SLOT(browse()));
	
	comboDoubleClick->addItems(QStringList() << tr("Switches to transfer details") << tr("Switches to the graph")
			<< tr("Opens the file") << tr("Opens the parent directory") );
	
	comboCloseCurrent->addItems(QStringList() << tr("switch to the next tab") << tr("switch to the previous active tab"));
	
	comboLinkSeparator->addItems(QStringList() << tr("a newline") << tr("whitespace characters"));
}

void SettingsGeneralForm::load()
{
	lineDestination->setText( g_settings->value("defaultdir", getSettingsDefault("defaultdir")).toString() );
	lineFileExec->setText( g_settings->value("fileexec", getSettingsDefault("fileexec")).toString() );
	
	checkTrayIcon->setChecked( g_settings->value("trayicon", getSettingsDefault("trayicon")).toBool() );
	checkHideMinimize->setChecked( g_settings->value("hideminimize", getSettingsDefault("hideminimize")).toBool() );
	checkHideClose->setChecked( g_settings->value("hideclose", getSettingsDefault("hideclose")).toBool() );
	
	spinGraphMinutes->setValue( g_settings->value("graphminutes", getSettingsDefault("graphminutes")).toInt() );
	
	comboDoubleClick->setCurrentIndex( g_settings->value("transfer_dblclk", getSettingsDefault("transfer_dblclk")).toInt() );
	comboCloseCurrent->setCurrentIndex( g_settings->value("tab_onclose", getSettingsDefault("transfer_dblclk")).toInt() );
	comboLinkSeparator->setCurrentIndex( g_settings->value("link_separator", getSettingsDefault("link_separator")).toInt() );
}

bool SettingsGeneralForm::accept()
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
	
	return true;
}
void SettingsGeneralForm::accepted()
{
	g_settings->setValue("defaultdir", lineDestination->text());
	g_settings->setValue("execfile", lineFileExec->text());
	
	g_settings->setValue("trayicon", checkTrayIcon->isChecked());
	g_settings->setValue("hideminimize", checkHideMinimize->isChecked());
	g_settings->setValue("hideclose", checkHideClose->isChecked());
	
	g_settings->setValue("graphminutes", spinGraphMinutes->value());
	g_settings->setValue("transfer_dblclk", comboDoubleClick->currentIndex());
	g_settings->setValue("tab_onclose", comboCloseCurrent->currentIndex());
	g_settings->setValue("link_separator", comboLinkSeparator->currentIndex());
}

void SettingsGeneralForm::browse()
{
	QString dir = QFileDialog::getExistingDirectory(0, tr("Choose directory"), lineDestination->text());
	if(!dir.isNull())
		lineDestination->setText(dir);
}
