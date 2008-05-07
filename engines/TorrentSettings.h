#ifndef TORRENTSETTINGS_H
#define TORRENTSETTINGS_H
#include <QObject>
#include "fatrat.h"
#include "WidgetHostChild.h"
#include "ui_SettingsTorrentForm.h"

class TorrentSettings : public QObject, public WidgetHostChild, Ui_SettingsTorrentForm
{
Q_OBJECT
public:
	TorrentSettings(QWidget* w, QObject* p);
	static WidgetHostChild* create(QWidget* w, QObject* p) { return new TorrentSettings(w, p); }
	virtual void load();
	virtual void accepted();
public slots:
	void cleanup();
private:
	QList<Proxy> m_listProxy;
};

#endif
