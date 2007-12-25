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
		
		comboAllocation->addItems( QStringList() << tr("Full") << tr("Sparse") << tr("Compact") );
	}
	
	virtual void load()
	{
		spinListenStart->setValue(g_settings->value("torrent/listen_start", getSettingsDefault("torrent/listen_start")).toInt());
		spinListenEnd->setValue(g_settings->value("torrent/listen_end", getSettingsDefault("torrent/listen_end")).toInt());
		spinRatio->setValue(g_settings->value("torrent/maxratio", getSettingsDefault("torrent/maxratio")).toDouble());
		
		spinConnections->setValue(g_settings->value("torrent/maxconnections", getSettingsDefault("torrent/maxconnections")).toInt());
		spinUploads->setValue(g_settings->value("torrent/maxuploads", getSettingsDefault("torrent/maxuploads")).toInt());
		spinConnectionsLocal->setValue(g_settings->value("torrent/maxconnections_loc", getSettingsDefault("torrent/maxconnections_loc")).toInt());
		spinUploadsLocal->setValue(g_settings->value("torrent/maxuploads_loc", getSettingsDefault("torrent/maxuploads_loc")).toInt());
		
		spinFiles->setValue(g_settings->value("torrent/maxfiles", getSettingsDefault("torrent/maxfiles")).toInt());
		checkDHT->setChecked(g_settings->value("torrent/dht", getSettingsDefault("torrent/dht")).toBool());
		checkPEX->setChecked(g_settings->value("torrent/pex", getSettingsDefault("torrent/pex")).toBool());
		comboAllocation->setCurrentIndex(g_settings->value("torrent/allocation", getSettingsDefault("torrent/allocation")).toInt());
	}
	
	virtual void accepted()
	{
		g_settings->setValue("torrent/listen_start", spinListenStart->value());
		g_settings->setValue("torrent/listen_end", spinListenEnd->value());
		g_settings->setValue("torrent/maxratio", spinRatio->value());
		
		g_settings->setValue("torrent/maxconnections", spinConnections->value());
		g_settings->setValue("torrent/maxuploads", spinUploads->value());
		g_settings->setValue("torrent/maxconnections_loc", spinConnectionsLocal->value());
		g_settings->setValue("torrent/maxuploads_loc", spinUploadsLocal->value());
		
		g_settings->setValue("torrent/maxfiles", spinFiles->value());
		g_settings->setValue("torrent/dht", checkDHT->isChecked());
		g_settings->setValue("torrent/pex", checkPEX->isChecked());
		g_settings->setValue("torrent/allocation", comboAllocation->currentIndex());
		
		TorrentDownload::applySettings();
	}
};

#endif
