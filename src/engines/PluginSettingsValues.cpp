/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2026 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 3 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#include "PluginSettingsValues.h"

#include <QSet>
#include <QtDebug>

namespace PluginSettingsValues
{

static bool supported(const QString& type)
{
	static const QSet<QString> types = { "text", "password", "checkbox", "combo", "group", "order" };
	return types.contains(type);
}

static bool isBool(const QString& type)
{
	return type == QLatin1String("checkbox") || type == QLatin1String("group");
}

static QJsonArray filter(const QJsonArray& config, QSet<QString>& reported)
{
	QJsonArray rv;

	for (const QJsonValue& v : config)
	{
		QJsonObject item = v.toObject();
		QString type = item.value("type").toString();

		if (!supported(type))
		{
			if (!reported.contains(type))
			{
				reported << type;
				qDebug() << "Unknown plugin config item type:" << type;
			}
			continue;
		}

		if (type == QLatin1String("group"))
			item["items"] = filter(item.value("items").toArray(), reported);
		rv << item;
	}
	return rv;
}

QJsonArray settingsItems(const QJsonArray& config)
{
	QSet<QString> reported;
	return filter(config, reported);
}

// (key, type) of every keyed item, inside groups too.
static QList<QPair<QString, QString> > keyed(const QJsonArray& items)
{
	QList<QPair<QString, QString> > rv;

	for (const QJsonValue& v : items)
	{
		QJsonObject item = v.toObject();
		QString key = item.value("key").toString();
		QString type = item.value("type").toString();

		if (!key.isEmpty())
			rv << qMakePair(key, type);
		if (type == QLatin1String("group"))
			rv << keyed(item.value("items").toArray());
	}
	return rv;
}

QJsonObject load(const QJsonArray& items, ValueGetter value, ArrayGetter array)
{
	QJsonObject rv;

	for (const QPair<QString, QString>& k : keyed(items))
	{
		if (k.second == QLatin1String("order"))
		{
			QJsonArray order;

			for (const QMap<QString, QVariant>& entry : array(k.first))
				order << entry.value(QLatin1String("item")).toString();
			rv[k.first] = order;
		}
		else if (isBool(k.second))
			rv[k.first] = value(k.first, false).toBool();
		else
			rv[k.first] = value(k.first, QVariant()).toString();
	}
	return rv;
}

void save(const QJsonArray& items, const QJsonObject& values, ValueSetter setValue, ArraySetter setArray)
{
	for (const QPair<QString, QString>& k : keyed(items))
	{
		if (!values.contains(k.first))
			continue;

		QJsonValue v = values.value(k.first);

		if (k.second == QLatin1String("order"))
		{
			QList<QMap<QString, QVariant> > entries;

			for (const QJsonValue& o : v.toArray())
			{
				QMap<QString, QVariant> entry;

				entry[QLatin1String("item")] = o.toString();
				entries << entry;
			}
			setArray(k.first, entries);
		}
		else if (isBool(k.second))
			setValue(k.first, v.toBool());
		else
			setValue(k.first, v.toString());
	}
}

}
