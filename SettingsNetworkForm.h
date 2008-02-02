#ifndef SETTINGSNETWORKFORM_H
#define SETTINGSNETWORKFORM_H
#include "ui_SettingsNetworkForm.h"
#include "WidgetHostChild.h"
#include "fatrat.h"

class SettingsNetworkForm : public QObject, public WidgetHostChild, Ui_SettingsNetworkForm
{
Q_OBJECT
public:
	SettingsNetworkForm(QWidget* w, QObject* parent);
	virtual void load();
	virtual void accepted();
public slots:
	void proxyAdd();
	void proxyEdit();
	void proxyDelete();
private:
	QList<Proxy> m_listProxy;
};

#endif
