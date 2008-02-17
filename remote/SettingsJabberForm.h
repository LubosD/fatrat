#ifndef SETTINGSJABBERFORM_H
#define SETTINGSJABBERFORM_H
#include <QObject>
#include "fatrat.h"
#include "config.h"
#include "WidgetHostChild.h"
#include "ui_SettingsJabberForm.h"

#ifndef WITH_JABBER
#	error This file is not supposed to be included!
#endif

class SettingsJabberForm : public QObject, public WidgetHostChild, Ui_SettingsJabberForm
{
Q_OBJECT
public:
	SettingsJabberForm(QWidget* w, QObject* parent);
	virtual void load();
	virtual void accepted();
private:
	QList<Proxy> m_listProxy;
};

#endif
