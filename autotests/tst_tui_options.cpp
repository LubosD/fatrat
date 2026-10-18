#include "Dialog.h"
#include "OptionsForm.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>

using namespace tui;

static QJsonArray fields(const char* json) { return QJsonDocument::fromJson(json).array(); }
static QJsonObject object(const char* json) { return QJsonDocument::fromJson(json).object(); }

static const char* Every = R"([
	{ "type": "label", "title": "Handled by a plugin" },
	{ "type": "text", "key": "name", "title": "Target file name" },
	{ "type": "password", "key": "pw", "title": "Password" },
	{ "type": "checkbox", "key": "super", "title": "Use super seeding" },
	{ "type": "combo", "key": "mode", "title": "FTP mode", "options": [ { "value": "active", "title": "Active mode" }, { "value": "passive" } ] },
	{ "type": "number", "key": "ratio", "title": "Seed ratio", "min": 0, "decimals": 2 },
	{ "type": "number", "key": "mb", "title": "Seed until uploaded", "min": 0, "max": 100, "suffix": "MB" },
	{ "type": "list", "key": "pick", "title": "Pick one", "rows": [ { "value": "a" } ] }
])";

static const char* Values = R"({ "name": "a.bin", "pw": "s", "super": false, "mode": "passive", "ratio": 1.5, "mb": 0, "pick": "a" })";

class TestTuiOptions : public QObject
{
	Q_OBJECT
private slots:
	void addsAFieldPerType()
	{
		FormPanel p;
		options::addFields(p, fields(Every), object(Values));

		QCOMPARE(p.text("opt:name"), QString("a.bin"));
		QVERIFY(p.field("opt:pw")->secret);
		QCOMPARE(p.field("opt:super")->type, FormField::Check);
		QCOMPARE(p.field("opt:mode")->type, FormField::Select);
		QCOMPARE(p.field("opt:mode")->options, QStringList({ "Active mode", "passive" }));
		QCOMPARE(p.value("opt:mode"), QString("passive"));
		QCOMPARE(p.text("opt:ratio"), QString("1.5"));
		QCOMPARE(p.field("opt:mb")->suffix, QString("MB"));
		QVERIFY(!p.field("opt:pick"));   // not editable here: a note instead
		QVERIFY(p.contentHeight(80) > 0);
	}

	void untouchedFieldsChangeNothing()
	{
		FormPanel p;
		options::addFields(p, fields(Every), object(Values));
		QJsonObject out;
		QCOMPARE(options::changed(p, fields(Every), object(Values), &out), QString());
		QVERIFY(out.isEmpty());
	}

	void onlyChangedValuesAreReturnedTyped()
	{
		FormPanel p;
		options::addFields(p, fields(Every), object(Values));
		p.setText("opt:name", "b.bin");
		p.field("opt:super")->checked = true;
		p.field("opt:mode")->index = 0;
		p.setText("opt:ratio", "1.50");   // the same number: not a change
		p.setText("opt:mb", "40");

		QJsonObject out;
		QCOMPARE(options::changed(p, fields(Every), object(Values), &out), QString());
		QCOMPARE(out, object(R"({ "name": "b.bin", "super": true, "mode": "active", "mb": 40 })"));
		QVERIFY(out["mb"].isDouble());
	}

	void aMissingNumberLeftEmptyIsNoChange()
	{
		const QJsonArray f = fields(R"([ { "type": "number", "key": "n", "title": "N" } ])");
		for (const char* values : { "{}", R"({ "n": "x" })" })
		{
			FormPanel p;
			options::addFields(p, f, object(values));
			QCOMPARE(p.text("opt:n"), QString());
			QJsonObject out;
			QCOMPARE(options::changed(p, f, object(values), &out), QString());
			QVERIFY(out.isEmpty());
		}
	}

	void anUnknownComboValueIsNoChangeUntilPicked()
	{
		const QJsonArray f = fields(R"([ { "type": "combo", "key": "c", "title": "C", "options": [ { "value": "a" }, { "value": "b" } ] } ])");
		for (const char* values : { "{}", R"({ "c": "gone" })" })
		{
			FormPanel p;
			options::addFields(p, f, object(values));
			QJsonObject out;
			QCOMPARE(options::changed(p, f, object(values), &out), QString());
			QVERIFY(out.isEmpty());

			p.field("opt:c")->index = 1;
			QCOMPARE(options::changed(p, f, object(values), &out), QString());
			QCOMPARE(out, object(R"({ "c": "b" })"));
		}
	}

	void anUntouchedNumberRoundTrips()
	{
		const QJsonArray f = fields(R"([ { "type": "number", "key": "n", "title": "N", "decimals": 2 } ])");
		const QJsonObject values{ { "n", 0.1 + 0.2 } };   // 0.30000000000000004
		FormPanel p;
		options::addFields(p, f, values);
		QJsonObject out;
		QCOMPARE(options::changed(p, f, values, &out), QString());
		QVERIFY(out.isEmpty());
	}

	void aGroupBecomesANote()
	{
		FormPanel p;
		options::addFields(p, fields(R"([
			{ "type": "group", "title": "Advanced", "items": [] },
			{ "type": "group", "items": [] }
		])"), QJsonObject());
		QCOMPARE(p.contentHeight(80), 1);
	}

	void badNumbersAreRefused_data()
	{
		QTest::addColumn<QString>("field");
		QTest::addColumn<QString>("text");
		QTest::addColumn<QString>("message");

		QTest::newRow("not a number") << "opt:ratio" << "lots" << "Seed ratio must be a number";
		QTest::newRow("a fraction") << "opt:mb" << "1.5" << "Seed until uploaded must be a whole number";
		QTest::newRow("below min") << "opt:ratio" << "-1" << "Seed ratio must be at least 0";
		QTest::newRow("above max") << "opt:mb" << "101" << "Seed until uploaded must be at most 100";
	}

	void badNumbersAreRefused()
	{
		QFETCH(QString, field);
		QFETCH(QString, text);
		QFETCH(QString, message);

		FormPanel p;
		options::addFields(p, fields(Every), object(Values));
		p.setText(field, text);
		QJsonObject out;
		QCOMPARE(options::changed(p, fields(Every), object(Values), &out), message);
	}
};

QTEST_GUILESS_MAIN(TestTuiOptions)
#include "tst_tui_options.moc"
