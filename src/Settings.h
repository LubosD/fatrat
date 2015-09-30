/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 3 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#ifndef SETTINGS_H
#define SETTINGS_H
#include <QString>
#include <QIcon>
#include <QSettings>
#include "config.h"
#include "WidgetHostChild.h"
#include "DelayedIcon.h"

struct SettingsItem;
extern QVector<SettingsItem> g_settingsPages;
extern QSettings* g_settings;

struct SettingsItem
{
	SettingsItem() : lpfnCreate(0), pfnApply(0)
#ifdef WITH_WEBINTERFACE
			,webSettingsScript(0),  webSettingsIconURL(0)
#endif
	{}

	// icon to show in the settings dialog (GUI only)
	DelayedIcon icon;
	// title to show in the settings dialog (GUI and WebUI)
	QString title;
	// function to call to create a settings subdialog (GUI only)
	WidgetHostChild* (*lpfnCreate)(QWidget*, QObject*);
	// function to call to apply or settings changes (GUI and WebUI), optional
	void (*pfnApply)();

#ifdef WITH_WEBINTERFACE
	// script to initialize the settings subdialog (WebUI only)
	const char* webSettingsScript;
	// URL of the settings icon (WebUI only)
	const char* webSettingsIconURL;
#endif
};

void initSettingsPages();
void addSettingsPage(const SettingsItem& i);
void applyAllSettings();

QVariant getSettingsDefault(QString id);
QVariant getSettingsValue(QString id, QVariant def = QVariant());
QList<QMap<QString, QVariant> > getSettingsArray(QString id);
void setSettingsValue(QString id, QVariant value);
void setSettingsArray(QString id, QList<QMap<QString, QVariant> >& value);

void initSettingsDefaults(QString manualPath = QString());
void exitSettings();

#endif
