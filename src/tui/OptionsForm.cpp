#include "OptionsForm.h"
#include "Dialog.h"
#include <QObject>
#include <cmath>

namespace tui
{
namespace options
{
static QString idOf(const QString& key)
{
	return "opt:" + key;
}

// Text that reads back as the same double, so an untouched field is never a change. 16 digits
// keeps whole numbers up to 1e15 free of exponents (the shortest form prints 100000 as 1e+05);
// 17 digits always round-trip, for the doubles 16 do not.
static QString numberText(double v)
{
	const QString text = QString::number(v, 'g', 16);
	return text.toDouble() == v ? text : QString::number(v, 'g', 17);
}

static QStringList comboValues(const QJsonObject& f)
{
	QStringList vals;
	for (const QJsonValue& o : f["options"].toArray())
		vals << o["value"].toString();
	return vals;
}

// a value that is missing or not among the options shows as the first one
static int comboStart(const QJsonObject& f, const QJsonValue& value)
{
	return qMax(0, int(comboValues(f).indexOf(value.toString())));
}

static QString nameOf(const QJsonObject& f)
{
	const QString title = f["title"].toString();
	return title.isEmpty() ? f["key"].toString() : title;
}

void addFields(FormPanel& panel, const QJsonArray& fields, const QJsonObject& values)
{
	for (const QJsonValue& v : fields)
	{
		const QJsonObject f = v.toObject();
		const QString type = f["type"].toString(), key = f["key"].toString(), title = f["title"].toString();
		const QJsonValue value = values.value(key);

		if (type == "label")
			panel.addLabel(title);
		else if (key.isEmpty() && type != "group")
			continue;
		else if (type == "text" || type == "password")
			panel.addText(idOf(key), title, value.toString()).secret = type == "password";
		else if (type == "number")
			panel.addText(idOf(key), title, value.isDouble() ? numberText(value.toDouble()) : QString(), f["suffix"].toString());
		else if (type == "checkbox")
			panel.addCheck(idOf(key), title, value.toBool());
		else if (type == "combo")
		{
			QStringList labels;
			for (const QJsonValue& o : f["options"].toArray())
			{
				const QString t = o["title"].toString();
				labels << (t.isEmpty() ? o["value"].toString() : t);
			}
			panel.addSelect(idOf(key), title, labels, comboValues(f), comboStart(f, value));
		}
		else if (!nameOf(f).isEmpty())
			panel.addLabel(QObject::tr("%1: edit in the desktop or web interface").arg(nameOf(f)));
	}
}

QString changed(const FormPanel& panel, const QJsonArray& fields, const QJsonObject& values, QJsonObject* out)
{
	QJsonObject result;

	for (const QJsonValue& v : fields)
	{
		const QJsonObject f = v.toObject();
		const QString type = f["type"].toString(), key = f["key"].toString(), id = idOf(key);
		const QJsonValue old = values.value(key);

		if (key.isEmpty())
			continue;

		if (type == "text" || type == "password")
		{
			if (panel.text(id) != old.toString())
				result[key] = panel.text(id);
		}
		else if (type == "checkbox")
		{
			if (panel.checked(id) != old.toBool())
				result[key] = panel.checked(id);
		}
		else if (type == "combo")
		{
			// only what the user picked: the first option standing in for an unknown value is not a change
			if (panel.index(id) != comboStart(f, old))
				result[key] = panel.value(id);
		}
		else if (type == "number")
		{
			const QString text = panel.text(id).trimmed();

			// untouched, including a box left empty because the server had no number for it
			if (text == (old.isDouble() ? numberText(old.toDouble()) : QString()))
				continue;

			// the same checks as FormValidator, so the user gets the message before anything is sent
			bool ok = false;
			const double d = text.toDouble(&ok);
			const QString name = nameOf(f);

			if (!ok || !std::isfinite(d))
				return QObject::tr("%1 must be a number").arg(name);
			if (f["decimals"].toInt() <= 0 && d != std::floor(d))
				return QObject::tr("%1 must be a whole number").arg(name);
			if (f.contains("min") && d < f["min"].toDouble())
				return QObject::tr("%1 must be at least %2").arg(name, numberText(f["min"].toDouble()));
			if (f.contains("max") && d > f["max"].toDouble())
				return QObject::tr("%1 must be at most %2").arg(name, numberText(f["max"].toDouble()));
			if (!old.isDouble() || old.toDouble() != d)
				result[key] = d;
		}
	}

	*out = result;
	return QString();
}
}
}
