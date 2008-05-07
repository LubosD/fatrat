#ifndef SETTINGS_H
#define SETTINGS_H
#include <QString>
#include <QIcon>
#include "WidgetHostChild.h"

struct SettingsItem
{
	QIcon icon;
	WidgetHostChild* (*lpfnCreate)(QWidget*, QObject*);
};

void initSettingsPages();
void addSettingsPage(const SettingsItem& i);

QVariant getSettingsDefault(QString id);
QVariant getSettingsValue(QString id);

void initSettingsDefaults();
void exitSettings();

#endif
