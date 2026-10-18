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

static QString numberText(double v)
{
	return QString::number(v, 'g', 15);
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
		else if (key.isEmpty())
			continue;
		else if (type == "text" || type == "password")
			panel.addText(idOf(key), title, value.toString()).secret = type == "password";
		else if (type == "number")
			panel.addText(idOf(key), title, value.isDouble() ? numberText(value.toDouble()) : QString(), f["suffix"].toString());
		else if (type == "checkbox")
			panel.addCheck(idOf(key), title, value.toBool());
		else if (type == "combo")
		{
			QStringList labels, vals;
			for (const QJsonValue& o : f["options"].toArray())
			{
				const QString val = o["value"].toString(), t = o["title"].toString();
				vals << val;
				labels << (t.isEmpty() ? val : t);
			}
			panel.addSelect(idOf(key), title, labels, vals, qMax(0, int(vals.indexOf(value.toString()))));
		}
		else
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
			if (panel.value(id) != old.toString())
				result[key] = panel.value(id);
		}
		else if (type == "number")
		{
			// the same checks as FormValidator, so the user gets the message before anything is sent
			bool ok = false;
			const double d = panel.text(id).trimmed().toDouble(&ok);
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
