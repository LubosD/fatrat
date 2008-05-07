#ifndef SETTINGSQUEUEFORM_H
#define SETTINGSQUEUEFORM_H
#include "ui_SettingsQueueForm.h"
#include "WidgetHostChild.h"

class SettingsQueueForm : public QObject, public WidgetHostChild, Ui_SettingsQueueForm
{
Q_OBJECT
public:
	SettingsQueueForm(QWidget* w, QObject* parent);
	virtual void load();
	virtual void accepted();
	virtual bool accept();
	static WidgetHostChild* create(QWidget* w, QObject* parent) { return new SettingsQueueForm(w, parent); }
public slots:
};

#endif
