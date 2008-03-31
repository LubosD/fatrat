#ifndef TORRENTSETTINGS_H
#define TORRENTSETTINGS_H
#include <QObject>
#include <QSettings>
#include "fatrat.h"
#include "WidgetHostChild.h"
#include "ui_SettingsTorrentForm.h"

extern QSettings* g_settings;

class TorrentSettings : public QObject, public WidgetHostChild, Ui_SettingsTorrentForm
{
Q_OBJECT
public:
	TorrentSettings(QWidget* w);
	virtual void load();
	virtual void accepted();
public slots:
	void cleanup();
private:
	QList<Proxy> m_listProxy;
};

#endif
