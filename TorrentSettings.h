#ifndef TORRENTSETTINGS_H
#define TORRENTSETTINGS_H
#include <QObject>
#include <QSettings>
#include "fatrat.h"
#include "WidgetHostChild.h"
#include "TorrentDownload.h"
#include "ui_SettingsTorrentForm.h"

extern QSettings* g_settings;

class TorrentSettings : public QObject, public WidgetHostChild, Ui_SettingsTorrentForm
{
Q_OBJECT
public:
	TorrentSettings(QWidget* w)
	{
		setupUi(w);
	}
	
	virtual void load()
	{
		g_settings->beginGroup("torrent");
		spinListenStart->setValue(g_settings->value("listen_start", getSettingsDefault("torrent/listen_start")).toInt());
		spinListenEnd->setValue(g_settings->value("listen_end", getSettingsDefault("torrent/listen_end")).toInt());
		spinRatio->setValue(g_settings->value("maxratio", getSettingsDefault("torrent/maxratio")).toDouble());
		spinConnections->setValue(g_settings->value("maxconnections", getSettingsDefault("torrent/maxconnections")).toInt());
		spinUploads->setValue(g_settings->value("maxuploads", getSettingsDefault("torrent/maxuploads")).toInt());
		checkDHT->setChecked(g_settings->value("dht", getSettingsDefault("torrent/dht")).toBool());
		checkPEX->setChecked(g_settings->value("pex", getSettingsDefault("torrent/pex")).toBool());
		g_settings->endGroup();
	}
	
	virtual void accepted()
	{
		g_settings->beginGroup("torrent");
		g_settings->setValue("listen_start", spinListenStart->value());
		g_settings->setValue("listen_end", spinListenEnd->value());
		g_settings->setValue("maxratio", spinRatio->value());
		g_settings->setValue("maxconnections", spinConnections->value());
		g_settings->setValue("maxuploads", spinUploads->value());
		g_settings->setValue("dht", checkDHT->isChecked());
		g_settings->setValue("pex", checkDHT->isChecked());
		g_settings->endGroup();
		
		TorrentDownload::applySettings();
	}
};

#endif
