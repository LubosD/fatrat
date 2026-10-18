#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include "plugins/FormValidator.h"

/* The form vocabulary, checked both ways: what a plugin may ask, and what a
 * client may answer. */
class tst_formvalidator : public QObject
{
	Q_OBJECT
private slots:
	void acceptsEveryFieldType();
	void rejectsMalformedForms_data();
	void rejectsMalformedForms();
	void acceptsAnAnswerAndDropsExtras();
	void rejectsInvalidAnswers_data();
	void rejectsInvalidAnswers();
	void checksNumberFields_data();
	void checksNumberFields();
	void checksNumberValues_data();
	void checksNumberValues();
	void checkValuesAcceptsPartialUpdates();
	void checkValuesRejects_data();
	void checkValuesRejects();
};

static QJsonObject obj(const char* json)
{
	QJsonParseError error;
	QJsonDocument doc = QJsonDocument::fromJson(json, &error);
	if (error.error != QJsonParseError::NoError)
		qFatal("bad test JSON: %s", qPrintable(error.errorString()));
	return doc.object();
}

static const char* EveryType = R"({
	"title": "All of them",
	"fields": [
		{ "type": "label", "title": "Just text" },
		{ "type": "text", "key": "t", "title": "Text", "default": "x" },
		{ "type": "password", "key": "p" },
		{ "type": "checkbox", "key": "c" },
		{ "type": "combo", "key": "o", "options": [ { "value": "a" }, { "value": "b", "title": "B" } ] },
		{ "type": "directory", "key": "d" },
		{ "type": "list", "key": "l", "columns": ["Name"], "rows": [ { "value": "r1", "cells": ["one"] }, { "value": "r2" } ] },
		{ "type": "checklist", "key": "m", "rows": [ { "value": "r1", "checked": true }, { "value": "r2" } ] },
		{ "type": "group", "title": "G", "key": "g", "items": [ { "type": "text", "key": "inner" } ] }
	]
})";

void tst_formvalidator::acceptsEveryFieldType()
{
	QCOMPARE(FormValidator::checkForm(obj(EveryType)), QString());
}

void tst_formvalidator::rejectsMalformedForms_data()
{
	QTest::addColumn<QString>("form");

	QTest::newRow("no fields") << R"({ "title": "x" })";
	QTest::newRow("field not an object") << R"({ "fields": [ 1 ] })";
	QTest::newRow("unknown type") << R"({ "fields": [ { "type": "bogus", "key": "a" } ] })";
	QTest::newRow("missing key") << R"({ "fields": [ { "type": "text" } ] })";
	QTest::newRow("duplicate key") << R"({ "fields": [ { "type": "text", "key": "a" }, { "type": "checkbox", "key": "a" } ] })";
	QTest::newRow("duplicate key in a group") << R"({ "fields": [ { "type": "text", "key": "a" }, { "type": "group", "items": [ { "type": "text", "key": "a" } ] } ] })";
	QTest::newRow("group without items") << R"({ "fields": [ { "type": "group", "title": "G" } ] })";
	QTest::newRow("combo without options") << R"({ "fields": [ { "type": "combo", "key": "a" } ] })";
	QTest::newRow("list without rows") << R"({ "fields": [ { "type": "list", "key": "a", "rows": [] } ] })";
	QTest::newRow("row without a value") << R"({ "fields": [ { "type": "checklist", "key": "a", "rows": [ { "cells": ["x"] } ] } ] })";
	QTest::newRow("duplicate row value") << R"({ "fields": [ { "type": "list", "key": "a", "rows": [ { "value": "x" }, { "value": "x" } ] } ] })";
}

void tst_formvalidator::rejectsMalformedForms()
{
	QFETCH(QString, form);
	QVERIFY(!FormValidator::checkForm(obj(form.toUtf8().constData())).isEmpty());
}

void tst_formvalidator::acceptsAnAnswerAndDropsExtras()
{
	QJsonObject normalized;
	QString error = FormValidator::checkAnswer(obj(EveryType), obj(R"({
		"t": "hello", "p": "", "c": true, "o": "b", "d": "/tmp", "l": "r2", "m": ["r1", "r2"],
		"g": false, "inner": "in", "junk": 42
	})"), &normalized);

	QCOMPARE(error, QString());
	QCOMPARE(normalized.size(), 9);
	QVERIFY(!normalized.contains("junk"));
	QCOMPARE(normalized.value("m").toArray().size(), 2);
}

void tst_formvalidator::rejectsInvalidAnswers_data()
{
	QTest::addColumn<QString>("answer");

	const char* valid = R"("t": "x", "p": "", "c": true, "o": "a", "d": "/", "l": "r1", "m": [], "g": true, "inner": "")";
	auto with = [valid](const char* replace, const char* by) {
		QString s = QString("{ %1 }").arg(valid);
		return s.replace(replace, by);
	};

	QTest::newRow("missing key") << with(R"("t": "x", )", "");
	QTest::newRow("text not a string") << with(R"("t": "x")", R"("t": 5)");
	QTest::newRow("checkbox not a bool") << with(R"("c": true)", R"("c": "yes")");
	QTest::newRow("group not a bool") << with(R"("g": true)", R"("g": 1)");
	QTest::newRow("combo not an option") << with(R"("o": "a")", R"("o": "z")");
	QTest::newRow("list not a row") << with(R"("l": "r1")", R"("l": "r9")");
	QTest::newRow("checklist not an array") << with(R"("m": [])", R"("m": "r1")");
	QTest::newRow("checklist unknown row") << with(R"("m": [])", R"("m": ["r9"])");
	QTest::newRow("checklist duplicate") << with(R"("m": [])", R"("m": ["r1", "r1"])");
}

void tst_formvalidator::rejectsInvalidAnswers()
{
	QFETCH(QString, answer);
	QJsonObject normalized;
	QVERIFY(!FormValidator::checkAnswer(obj(EveryType), obj(answer.toUtf8().constData()), &normalized).isEmpty());
}

void tst_formvalidator::checksNumberFields_data()
{
	QTest::addColumn<QString>("form");
	QTest::addColumn<bool>("valid");

	QTest::newRow("bare") << R"({ "fields": [ { "type": "number", "key": "n" } ] })" << true;
	QTest::newRow("all attributes") << R"({ "fields": [ { "type": "number", "key": "n", "min": 0, "max": 10, "decimals": 2, "suffix": "MB" } ] })" << true;
	QTest::newRow("no key") << R"({ "fields": [ { "type": "number" } ] })" << false;
	QTest::newRow("min not a number") << R"({ "fields": [ { "type": "number", "key": "n", "min": "0" } ] })" << false;
	QTest::newRow("max not a number") << R"({ "fields": [ { "type": "number", "key": "n", "max": true } ] })" << false;
	QTest::newRow("decimals not a number") << R"({ "fields": [ { "type": "number", "key": "n", "decimals": "2" } ] })" << false;
}

void tst_formvalidator::checksNumberFields()
{
	QFETCH(QString, form);
	QFETCH(bool, valid);
	QCOMPARE(FormValidator::checkForm(obj(form.toUtf8().constData())).isEmpty(), valid);
}

static const char* Numbers = R"({
	"fields": [
		{ "type": "number", "key": "whole", "min": 0, "max": 100 },
		{ "type": "number", "key": "ratio", "min": 0, "decimals": 2 }
	]
})";

void tst_formvalidator::checksNumberValues_data()
{
	QTest::addColumn<QString>("answer");
	QTest::addColumn<bool>("valid");

	QTest::newRow("in range") << R"({ "whole": 5, "ratio": 1.5 })" << true;
	QTest::newRow("at the limits") << R"({ "whole": 100, "ratio": 0 })" << true;
	QTest::newRow("a string") << R"({ "whole": "5", "ratio": 1 })" << false;
	QTest::newRow("below min") << R"({ "whole": -1, "ratio": 1 })" << false;
	QTest::newRow("above max") << R"({ "whole": 101, "ratio": 1 })" << false;
	QTest::newRow("a fraction without decimals") << R"({ "whole": 2.5, "ratio": 1 })" << false;
}

void tst_formvalidator::checksNumberValues()
{
	QFETCH(QString, answer);
	QFETCH(bool, valid);
	QJsonObject normalized;
	QCOMPARE(FormValidator::checkAnswer(obj(Numbers), obj(answer.toUtf8().constData()), &normalized).isEmpty(), valid);
}

void tst_formvalidator::checkValuesAcceptsPartialUpdates()
{
	QCOMPARE(FormValidator::checkValues(obj(EveryType), obj(R"({ "c": true })")), QString());
	QCOMPARE(FormValidator::checkValues(obj(EveryType), obj(R"({})")), QString());
	QCOMPARE(FormValidator::checkValues(obj(EveryType), obj(R"({ "t": "x", "o": "b", "inner": "y" })")), QString());
	QCOMPARE(FormValidator::checkValues(obj(Numbers), obj(R"({ "ratio": 0.25 })")), QString());
}

void tst_formvalidator::checkValuesRejects_data()
{
	QTest::addColumn<QString>("values");

	QTest::newRow("unknown key") << R"({ "nope": 1 })";
	QTest::newRow("label has no value") << R"({ "": "x" })";
	QTest::newRow("text not a string") << R"({ "t": 5 })";
	QTest::newRow("checkbox not a bool") << R"({ "c": "yes" })";
	QTest::newRow("combo not an option") << R"({ "o": "z" })";
	QTest::newRow("one good, one bad") << R"({ "t": "x", "c": 1 })";
}

void tst_formvalidator::checkValuesRejects()
{
	QFETCH(QString, values);
	QVERIFY(!FormValidator::checkValues(obj(EveryType), obj(values.toUtf8().constData())).isEmpty());
}

QTEST_GUILESS_MAIN(tst_formvalidator)
#include "tst_formvalidator.moc"
