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

#include "FormValidator.h"

#include <QJsonArray>
#include <QList>
#include <QSet>
#include <algorithm>
#include <cmath>

namespace
{
	const QSet<QString> KeyedTypes = {
		"text", "password", "checkbox", "combo", "directory", "list", "checklist", "number"
	};

	struct Keyed
	{
		QString key, type;
		QJsonObject item;
	};

	// The `value` of every entry of item[member] (a combo's options, a list's rows).
	QSet<QString> choiceValues(const QJsonObject& item, const QString& member)
	{
		QSet<QString> rv;

		for (const QJsonValue& c : item.value(member).toArray())
			rv << c.toObject().value("value").toString();
		return rv;
	}

	QString checkChoices(const QJsonObject& item, const QString& member, const QString& key)
	{
		QJsonArray choices = item.value(member).toArray();
		QSet<QString> values;

		if (choices.isEmpty())
			return QString("\"%1\" has no %2").arg(key, member);

		for (const QJsonValue& c : choices)
		{
			QJsonValue value = c.toObject().value("value");

			if (!value.isString())
				return QString("\"%1\" has an entry without a value").arg(key);
			if (values.contains(value.toString()))
				return QString("\"%1\" has the value \"%2\" twice").arg(key, value.toString());
			values << value.toString();
		}
		return QString();
	}

	QString checkNumberField(const QJsonObject& item, const QString& key)
	{
		for (const char* attribute : { "min", "max", "decimals" })
		{
			if (item.contains(attribute) && !item.value(attribute).isDouble())
				return QString("\"%1\" has a %2 that is not a number").arg(key, QLatin1String(attribute));
		}
		return QString();
	}

	// Walks the fields, groups included, collecting every keyed one.
	QString checkItems(const QJsonArray& items, QSet<QString>& keys, QList<Keyed>& keyed)
	{
		for (const QJsonValue& v : items)
		{
			if (!v.isObject())
				return "a field is not an object";

			QJsonObject item = v.toObject();
			QString type = item.value("type").toString();
			QString key = item.value("key").toString();

			if (type == "label")
				continue;

			if (type == "group")
			{
				if (!item.value("items").isArray())
					return "a group has no items";

				// A group with a key is a checkable one.
				if (!key.isEmpty())
				{
					if (keys.contains(key))
						return QString("the key \"%1\" is used twice").arg(key);
					keys << key;
					keyed << Keyed{key, type, item};
				}

				QString error = checkItems(item.value("items").toArray(), keys, keyed);
				if (!error.isEmpty())
					return error;
				continue;
			}

			if (!KeyedTypes.contains(type))
				return QString("unknown field type \"%1\"").arg(type);
			if (key.isEmpty())
				return QString("a %1 field has no key").arg(type);
			if (keys.contains(key))
				return QString("the key \"%1\" is used twice").arg(key);
			keys << key;

			QString error;
			if (type == "combo")
				error = checkChoices(item, "options", key);
			else if (type == "list" || type == "checklist")
				error = checkChoices(item, "rows", key);
			else if (type == "number")
				error = checkNumberField(item, key);
			if (!error.isEmpty())
				return error;

			keyed << Keyed{key, type, item};
		}
		return QString();
	}

	QString collect(const QJsonObject& form, QList<Keyed>& keyed)
	{
		QSet<QString> keys;

		if (!form.value("fields").isArray())
			return "the form has no fields";
		return checkItems(form.value("fields").toArray(), keys, keyed);
	}

	// What is wrong with `v` as the value of `f`, or an empty string.
	QString checkValue(const Keyed& f, const QJsonValue& v)
	{
		if (f.type == "checkbox" || f.type == "group")
		{
			if (!v.isBool())
				return QString("\"%1\" must be true or false").arg(f.key);
		}
		else if (f.type == "combo" || f.type == "list")
		{
			QString member = (f.type == "combo") ? "options" : "rows";

			if (!v.isString() || !choiceValues(f.item, member).contains(v.toString()))
				return QString("\"%1\" is not one of the choices").arg(f.key);
		}
		else if (f.type == "checklist")
		{
			QSet<QString> allowed = choiceValues(f.item, "rows"), seen;

			if (!v.isArray())
				return QString("\"%1\" must be a list").arg(f.key);

			for (const QJsonValue& e : v.toArray())
			{
				if (!e.isString() || !allowed.contains(e.toString()) || seen.contains(e.toString()))
					return QString("\"%1\" has a value that is not one of its rows").arg(f.key);
				seen << e.toString();
			}
		}
		else if (f.type == "number")
		{
			double d = v.toDouble();

			if (!v.isDouble() || !std::isfinite(d))
				return QString("\"%1\" must be a number").arg(f.key);
			if (f.item.contains("min") && d < f.item.value("min").toDouble())
				return QString("\"%1\" must be at least %2").arg(f.key).arg(f.item.value("min").toDouble());
			if (f.item.contains("max") && d > f.item.value("max").toDouble())
				return QString("\"%1\" must be at most %2").arg(f.key).arg(f.item.value("max").toDouble());
			if (f.item.value("decimals").toInt() <= 0 && d != std::floor(d))
				return QString("\"%1\" must be a whole number").arg(f.key);
		}
		else if (!v.isString())
			return QString("\"%1\" must be text").arg(f.key);

		return QString();
	}
}

QString FormValidator::checkForm(const QJsonObject& form)
{
	QList<Keyed> keyed;
	return collect(form, keyed);
}

QString FormValidator::checkAnswer(const QJsonObject& form, const QJsonObject& answer, QJsonObject* normalized)
{
	QList<Keyed> keyed;
	QString error = collect(form, keyed);
	QJsonObject result;

	if (!error.isEmpty())
		return error;

	for (const Keyed& f : keyed)
	{
		if (!answer.contains(f.key))
			return QString("no value for \"%1\"").arg(f.key);

		error = checkValue(f, answer.value(f.key));
		if (!error.isEmpty())
			return error;

		result.insert(f.key, answer.value(f.key));
	}

	if (normalized)
		*normalized = result;
	return QString();
}

QString FormValidator::checkValues(const QJsonObject& form, const QJsonObject& values)
{
	QList<Keyed> keyed;
	QString error = collect(form, keyed);

	if (!error.isEmpty())
		return error;

	for (auto it = values.begin(); it != values.end(); ++it)
	{
		auto f = std::find_if(keyed.begin(), keyed.end(), [&it](const Keyed& k) { return k.key == it.key(); });

		if (f == keyed.end())
			return QString("unknown option \"%1\"").arg(it.key());

		error = checkValue(*f, it.value());
		if (!error.isEmpty())
			return error;
	}
	return QString();
}
