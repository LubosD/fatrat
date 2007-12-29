#ifndef SETTINGSPROXYFORM_H
#define SETTINGSPROXYFORM_H
#include "ui_SettingsProxyForm.h"
#include "fatrat.h"
#include "WidgetHostChild.h"

class SettingsProxyForm : public QObject, public WidgetHostChild, Ui_SettingsProxyForm
{
Q_OBJECT
public:
	SettingsProxyForm(QWidget* w);
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
