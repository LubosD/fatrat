#include "config.h"
#include "fatrat.h"

#include "Settings.h"
#include <QSettings>
#include <QVector>
#include <QDir>

#include "SettingsGeneralForm.h"
#include "SettingsDropBoxForm.h"
#include "SettingsNetworkForm.h"
#include "SettingsQueueForm.h"
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
	si.lpfnCreate = SettingsGeneralForm::create;
	
	g_settingsPages << si;
	
	si.icon = QIcon(":/fatrat/queue.png");
	si.lpfnCreate = SettingsQueueForm::create;
	
	g_settingsPages << si;
	
	si.icon = QIcon(":/fatrat/miscellaneous.png");
	si.lpfnCreate = SettingsDropBoxForm::create;
	
	g_settingsPages << si;
	
	si.icon = QIcon(":/menu/network.png");
	si.lpfnCreate = SettingsNetworkForm::create;
	
	g_settingsPages << si;
	
	si.icon = QIcon(":/fatrat/rss.png");
	si.lpfnCreate = SettingsRssForm::create;
	
	g_settingsPages << si;
	
#ifdef WITH_JABBER
	si.icon = QIcon(":/fatrat/jabber.png");
	si.lpfnCreate = SettingsJabberForm::create;
	
	g_settingsPages << si;
#endif
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
	m_settingsDefaults = new QSettings(path, QSettings::IniFormat, qApp);
}

void exitSettings()
{
	delete g_settings;
}

QVariant getSettingsDefault(QString id)
{
	if(id == "defaultdir")
		return QDir::homePath();
	else
		return m_settingsDefaults->value(id);
}

