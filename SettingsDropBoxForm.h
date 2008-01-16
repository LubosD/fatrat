#ifndef SETTINGSDROPBOXFORM_H
#define SETTINGSDROPBOXFORM_H

#include "fatrat.h"
#include "WidgetHostChild.h"
#include "ui_SettingsDropBoxForm.h"
#include <QSettings>

extern QSettings* g_settings;

class SettingsDropBoxForm : public QObject, Ui_SettingsDropBoxForm, public WidgetHostChild
{
Q_OBJECT
public:
	SettingsDropBoxForm(QWidget* me, QObject* parent) : QObject(parent)
	{
		setupUi(me);
	}
	
	virtual void load()
	{
		checkUnhide->setChecked( g_settings->value("dropbox/unhide", getSettingsDefault("dropbox/unhide")).toBool() );
		spinHeight->setValue( g_settings->value("dropbox/height", getSettingsDefault("dropbox/height")).toInt() );
	}
	
	virtual void accepted()
	{
		g_settings->setValue("dropbox/unhide", checkUnhide->isChecked());
		g_settings->setValue("dropbox/height", spinHeight->value());
	}
};

#endif
