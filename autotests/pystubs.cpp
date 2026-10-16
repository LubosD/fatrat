/* Stand-ins for the parts of FatRat that the Python bridge calls into.
 *
 * The real Settings.cpp drags in the scheduler, the RSS fetcher and the
 * settings-page registry, none of which the bridge needs; this keeps the test
 * binary to the bridge itself.
 */
#include <QMap>
#include <QSettings>
#include <QString>
#include <QVariant>

#include "Settings.h"

QSettings* g_settings = 0;
QVector<SettingsItem> g_settingsPages;

static QMap<QString, QVariant>& store()
{
	static QMap<QString, QVariant> map;
	return map;
}

static QMap<QString, QList<QMap<QString, QVariant> > >& arrayStore()
{
	static QMap<QString, QList<QMap<QString, QVariant> > > map;
	return map;
}

QVariant getSettingsDefault(QString)
{
	return QVariant();
}

QVariant getSettingsValue(QString id, QVariant def)
{
	return store().contains(id) ? store()[id] : def;
}

void setSettingsValue(QString id, QVariant value)
{
	store()[id] = value;
}

QList<QMap<QString, QVariant> > getSettingsArray(QString id)
{
	return arrayStore().value(id);
}

void setSettingsArray(QString id, QList<QMap<QString, QVariant> >& value)
{
	arrayStore()[id] = value;
}
