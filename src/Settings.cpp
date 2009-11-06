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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#include "config.h"
#include "fatrat.h"

#include "Settings.h"
#include <QSettings>
#include <QVector>
#include <QDir>
#include <QMessageBox>
#include <iostream>

#include "SettingsGeneralForm.h"
#include "SettingsDropBoxForm.h"
#include "SettingsNetworkForm.h"
#include "SettingsQueueForm.h"
#include "SettingsSchedulerForm.h"
#include "rss/SettingsRssForm.h"

#ifdef WITH_JABBER
#	include "remote/SettingsJabberForm.h"
#endif

QVector<SettingsItem> g_settingsPages;
QSettings* g_settings = 0;

static QSettings* m_settingsDefaults = 0;

void initSettingsPages()
{
	SettingsItem si;
	
	si.icon = QIcon(":/fatrat/fatrat.png");
	si.title = QObject::tr("Main");
	si.lpfnCreate = SettingsGeneralForm::create;
	
	g_settingsPages << si;
	
	si.icon = QIcon(":/fatrat/queue.png");
	si.title = QObject::tr("Queue");
	si.lpfnCreate = SettingsQueueForm::create;
	
	g_settingsPages << si;
	
	si.icon = QIcon(":/fatrat/miscellaneous.png");
	si.title = QObject::tr("Drop-box");
	si.lpfnCreate = SettingsDropBoxForm::create;
	
	g_settingsPages << si;
	
	si.icon = QIcon(":/menu/network.png");
	si.title = QObject::tr("Network");
	si.lpfnCreate = SettingsNetworkForm::create;
	
	g_settingsPages << si;
	
	si.icon = QIcon(":/fatrat/rss.png");
	si.title = QObject::tr("RSS");
	si.lpfnCreate = SettingsRssForm::create;
	
	g_settingsPages << si;
	
#ifdef WITH_JABBER
	si.icon = QIcon(":/fatrat/jabber.png");
	si.title = QObject::tr("Jabber");
	si.lpfnCreate = SettingsJabberForm::create;
	
	g_settingsPages << si;
#endif
	
	si.icon = QIcon(":/fatrat/scheduler.png");
	si.title = QObject::tr("Scheduler");
	si.lpfnCreate = SettingsSchedulerForm::create;
	
	g_settingsPages << si;
}

void addSettingsPage(const SettingsItem& i)
{
	g_settingsPages << i;
}

QVariant getSettingsValue(QString id)
{
	return g_settings->value(id, getSettingsDefault(id));
}

void setSettingsValue(QString id, QVariant value)
{
	g_settings->setValue(id, value);
}

void initSettingsDefaults()
{
	g_settings = new QSettings;
	
	QLatin1String df("/defaults.conf");
	QString path = getDataFileDir("/data", df) + df;

	if (!QFile::exists(path)) {
		const char* error = "Your installation is incomplete.\nPlease learn how to install programs first!\n";
		std::cerr << error;

		if (getenv("DISPLAY"))
			QMessageBox::critical(0, "FatRat", error);
		exit(1);
	}

	m_settingsDefaults = new QSettings(path, QSettings::IniFormat, qApp);
}

void exitSettings()
{
	delete g_settings;
}

QVariant getSettingsDefault(QString id)
{
	return m_settingsDefaults->value(id);
}

