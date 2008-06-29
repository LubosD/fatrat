/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
USA.
*/

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
