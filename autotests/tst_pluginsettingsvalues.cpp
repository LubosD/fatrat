#include <QJsonDocument>
#include <QTest>

#include "engines/PluginSettingsValues.h"

/* The mapping between a plugin's config and stored settings, with fake
 * stores instead of the real settings. */
class tst_pluginsettingsvalues : public QObject
{
	Q_OBJECT
private slots:
	void dropsUnsupportedItemsEvenInGroups();
	void loadsEachType();
	void savesEachType();
	void savesNothingForMissingKeys();
	void roundTrips();
};

static QJsonArray config()
{
	return QJsonDocument::fromJson(R"([
		{ "type": "label", "title": "Read me" },
		{ "type": "text", "key": "t" },
		{ "type": "password", "key": "p" },
		{ "type": "checkbox", "key": "c" },
		{ "type": "combo", "key": "co", "options": [ { "value": "a" } ] },
		{ "type": "directory", "key": "d" },
		{ "type": "list", "key": "l", "rows": [] },
		{ "type": "group", "key": "g", "items": [
			{ "type": "text", "key": "inner" },
			{ "type": "checklist", "key": "cl", "rows": [] }
		] },
		{ "type": "order", "key": "o", "options": [ { "value": "x" }, { "value": "y" } ] }
	])").array();
}

struct Store
{
	QMap<QString, QVariant> values;
	QMap<QString, QList<QMap<QString, QVariant> > > arrays;

	PluginSettingsValues::ValueGetter getter()
	{
		return [this](const QString& k, const QVariant& def) { return values.value(k, def); };
	}
	PluginSettingsValues::ArrayGetter arrayGetter()
	{
		return [this](const QString& k) { return arrays.value(k); };
	}
	PluginSettingsValues::ValueSetter setter()
	{
		return [this](const QString& k, const QVariant& v) { values[k] = v; };
	}
	PluginSettingsValues::ArraySetter arraySetter()
	{
		return [this](const QString& k, const QList<QMap<QString, QVariant> >& v) { arrays[k] = v; };
	}
};

static QStringList keysOf(const QJsonArray& items)
{
	QStringList rv;

	for (const QJsonValue& v : items)
	{
		rv << v.toObject().value("type").toString();
		if (v.toObject().contains("items"))
			rv << "[" + keysOf(v.toObject().value("items").toArray()).join(",") + "]";
	}
	return rv;
}

void tst_pluginsettingsvalues::dropsUnsupportedItemsEvenInGroups()
{
	QJsonArray items = PluginSettingsValues::settingsItems(config());

	QCOMPARE(keysOf(items), QStringList({ "text", "password", "checkbox", "combo", "group", "[text]", "order" }));
}

void tst_pluginsettingsvalues::loadsEachType()
{
	Store store;
	store.values = { { "t", "text" }, { "p", "secret" }, { "c", true }, { "co", "a" }, { "g", true }, { "inner", "in" } };
	store.arrays["o"] = { { { "item", "y" } }, { { "item", "x" } } };

	QJsonObject v = PluginSettingsValues::load(PluginSettingsValues::settingsItems(config()),
		store.getter(), store.arrayGetter());

	QCOMPARE(v.value("t").toString(), QString("text"));
	QCOMPARE(v.value("p").toString(), QString("secret"));
	QCOMPARE(v.value("c").toBool(), true);
	QCOMPARE(v.value("co").toString(), QString("a"));
	QCOMPARE(v.value("g").toBool(), true);
	QCOMPARE(v.value("inner").toString(), QString("in"));
	QCOMPARE(v.value("o").toArray(), QJsonArray({ "y", "x" }));
	QCOMPARE(v.size(), 7);

	// Nothing stored: empty strings, false, an empty order.
	Store empty;
	v = PluginSettingsValues::load(PluginSettingsValues::settingsItems(config()),
		empty.getter(), empty.arrayGetter());
	QCOMPARE(v.value("t").toString(), QString());
	QCOMPARE(v.value("c").toBool(), false);
	QCOMPARE(v.value("g").toBool(), false);
	QCOMPARE(v.value("o").toArray(), QJsonArray());
}

void tst_pluginsettingsvalues::savesEachType()
{
	Store store;
	QJsonObject values = QJsonDocument::fromJson(R"({
		"t": "text", "p": "secret", "c": true, "co": "a", "g": false, "inner": "in", "o": ["y", "x"]
	})").object();

	PluginSettingsValues::save(PluginSettingsValues::settingsItems(config()), values,
		store.setter(), store.arraySetter());

	QCOMPARE(store.values.value("t"), QVariant(QString("text")));
	QCOMPARE(store.values.value("p"), QVariant(QString("secret")));
	QCOMPARE(store.values.value("c"), QVariant(true));
	QCOMPARE(store.values.value("co"), QVariant(QString("a")));
	QCOMPARE(store.values.value("g"), QVariant(false));
	QCOMPARE(store.values.value("inner"), QVariant(QString("in")));
	QCOMPARE(store.values.size(), 6);

	QList<QMap<QString, QVariant> > order = { { { "item", "y" } }, { { "item", "x" } } };
	QCOMPARE(store.arrays.value("o"), order);
	QCOMPARE(store.arrays.size(), 1);
}

void tst_pluginsettingsvalues::savesNothingForMissingKeys()
{
	Store store;

	PluginSettingsValues::save(PluginSettingsValues::settingsItems(config()),
		QJsonDocument::fromJson(R"({ "t": "only" })").object(), store.setter(), store.arraySetter());

	QCOMPARE(store.values.size(), 1);
	QVERIFY(store.arrays.isEmpty());
}

void tst_pluginsettingsvalues::roundTrips()
{
	QJsonArray items = PluginSettingsValues::settingsItems(config());
	Store a, b;

	a.values = { { "t", "text" }, { "c", true }, { "co", "a" }, { "inner", "in" } };
	a.arrays["o"] = { { { "item", "y" } }, { { "item", "x" } } };

	PluginSettingsValues::save(items, PluginSettingsValues::load(items, a.getter(), a.arrayGetter()),
		b.setter(), b.arraySetter());

	QCOMPARE(b.arrays, a.arrays);
	for (auto it = a.values.constBegin(); it != a.values.constEnd(); it++)
		QCOMPARE(b.values.value(it.key()), it.value());
}

QTEST_MAIN(tst_pluginsettingsvalues)
#include "tst_pluginsettingsvalues.moc"
