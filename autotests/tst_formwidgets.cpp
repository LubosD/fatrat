#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineEdit>
#include <QListWidget>
#include <QTest>
#include <QTreeWidget>

#include "FormWidgets.h"

/* The form vocabulary as Qt widgets, read back as JSON. Each keyed widget is
 * named after its key, which is how the tests (and nothing else) find it. */
class tst_formwidgets : public QObject
{
	Q_OBJECT
private slots:
	void startsWithTheDefaults();
	void givenValuesOverrideDefaults();
	void readsBackEdits();
	void orderKeepsTheSavedOrderThenNewOptions();
	void listsEveryKey();
};

static QJsonArray items()
{
	return QJsonDocument::fromJson(R"([
		{ "type": "label", "title": "Read me" },
		{ "type": "text", "key": "text", "title": "Text", "default": "t" },
		{ "type": "password", "key": "pw", "title": "Password" },
		{ "type": "checkbox", "key": "flag", "title": "Flag", "default": true },
		{ "type": "combo", "key": "combo", "title": "Combo", "default": "b",
		  "options": [ { "value": "a", "title": "A" }, { "value": "b" } ] },
		{ "type": "directory", "key": "dir", "title": "Directory", "default": "/tmp" },
		{ "type": "list", "key": "pick", "title": "Pick", "columns": ["Name", "Size"],
		  "rows": [ { "value": "r1", "cells": ["one", "1 B"] }, { "value": "r2", "cells": ["two", "2 B"] } ] },
		{ "type": "checklist", "key": "many", "title": "Many",
		  "rows": [ { "value": "r1" }, { "value": "r2", "checked": true } ] },
		{ "type": "group", "key": "grp", "title": "Group", "items": [ { "type": "text", "key": "inner" } ] },
		{ "type": "order", "key": "order", "title": "Order", "options": [ { "value": "x" }, { "value": "y" } ] }
	])").array();
}

void tst_formwidgets::startsWithTheDefaults()
{
	QWidget parent;
	FormWidgets form(items(), QJsonObject(), &parent);
	QJsonObject v = form.values();

	QCOMPARE(v.value("text").toString(), QString("t"));
	QCOMPARE(v.value("pw").toString(), QString());
	QCOMPARE(v.value("flag").toBool(), true);
	QCOMPARE(v.value("combo").toString(), QString("b"));
	QCOMPARE(v.value("dir").toString(), QString("/tmp"));
	QCOMPARE(v.value("pick").toString(), QString("r1")); // the first row, when there is no default
	QCOMPARE(v.value("many").toArray(), QJsonArray({ "r2" }));
	QCOMPARE(v.value("grp").toBool(), false);
	QCOMPARE(v.value("inner").toString(), QString());
	QCOMPARE(v.value("order").toArray(), QJsonArray({ "x", "y" }));
	QCOMPARE(v.size(), 10);
}

void tst_formwidgets::givenValuesOverrideDefaults()
{
	QWidget parent;
	QJsonObject values = QJsonDocument::fromJson(R"({
		"text": "given", "flag": false, "combo": "a", "pick": "r2", "many": ["r1"], "grp": true
	})").object();
	FormWidgets form(items(), values, &parent);
	QJsonObject v = form.values();

	QCOMPARE(v.value("text").toString(), QString("given"));
	QCOMPARE(v.value("flag").toBool(), false);
	QCOMPARE(v.value("combo").toString(), QString("a"));
	QCOMPARE(v.value("pick").toString(), QString("r2"));
	QCOMPARE(v.value("many").toArray(), QJsonArray({ "r1" }));
	QCOMPARE(v.value("grp").toBool(), true);
}

void tst_formwidgets::readsBackEdits()
{
	QWidget parent;
	FormWidgets form(items(), QJsonObject(), &parent);

	parent.findChild<QLineEdit*>("text")->setText("edited");
	parent.findChild<QLineEdit*>("dir")->setText("/srv");
	parent.findChild<QCheckBox*>("flag")->setChecked(false);
	parent.findChild<QComboBox*>("combo")->setCurrentIndex(0);
	QTreeWidget* pick = parent.findChild<QTreeWidget*>("pick");
	pick->setCurrentItem(pick->topLevelItem(1));
	QTreeWidget* many = parent.findChild<QTreeWidget*>("many");
	many->topLevelItem(0)->setCheckState(0, Qt::Checked);
	many->topLevelItem(1)->setCheckState(0, Qt::Unchecked);
	parent.findChild<QGroupBox*>("grp")->setChecked(true);

	QJsonObject v = form.values();
	QCOMPARE(v.value("text").toString(), QString("edited"));
	QCOMPARE(v.value("dir").toString(), QString("/srv"));
	QCOMPARE(v.value("flag").toBool(), false);
	QCOMPARE(v.value("combo").toString(), QString("a"));
	QCOMPARE(v.value("pick").toString(), QString("r2"));
	QCOMPARE(v.value("many").toArray(), QJsonArray({ "r1" }));
	QCOMPARE(v.value("grp").toBool(), true);
}

void tst_formwidgets::orderKeepsTheSavedOrderThenNewOptions()
{
	QWidget parent;
	QJsonArray order = QJsonDocument::fromJson(R"([
		{ "type": "order", "key": "o", "options": [ { "value": "x" }, { "value": "y" }, { "value": "z" } ] }
	])").array();
	FormWidgets form(order, QJsonDocument::fromJson(R"({ "o": ["z", "gone", "x"] })").object(), &parent);

	QCOMPARE(form.values().value("o").toArray(), QJsonArray({ "z", "x", "y" }));
}

void tst_formwidgets::listsEveryKey()
{
	QList<QPair<QString, QString>> keys = FormWidgets::keys(items());

	QCOMPARE(keys.size(), 10);
	QVERIFY(keys.contains(qMakePair(QString("grp"), QString("group"))));
	QVERIFY(keys.contains(qMakePair(QString("inner"), QString("text"))));
	QVERIFY(keys.contains(qMakePair(QString("order"), QString("order"))));
}

QTEST_MAIN(tst_formwidgets)
#include "tst_formwidgets.moc"
