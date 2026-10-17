#include <QJsonDocument>
#include <QLineEdit>
#include <QPointer>
#include <QTest>
#include <QTreeWidget>

#include "FormDialog.h"
#include "plugins/Task.h"
#include "plugins/TaskManager.h"

// From fatrat.cpp, which this test does not link.
QString formatTime(qulonglong secs)
{
	return QString::number(secs) + " s";
}

/* The desktop's side of a plugin question. */
class tst_formdialog : public QObject
{
	Q_OBJECT
private slots:
	void okAnswers();
	void invalidAnswerStaysOpen();
	void cancelDismisses();
	void closesWhenAnsweredElsewhere();
private:
	Task* asking(std::optional<QJsonObject>* got, bool* called);
};

Task* tst_formdialog::asking(std::optional<QJsonObject>* got, bool* called)
{
	Task* task = TaskManager::instance()->create("p.so", "Asks");

	task->ask(QJsonDocument::fromJson(R"({ "title": "Name?",
		"fields": [ { "type": "combo", "key": "c", "options": [ { "value": "a" } ] },
			    { "type": "text", "key": "name", "default": "x" } ] })").object(), 60000,
		[got, called](std::optional<QJsonObject> r) { *called = true; *got = r; });
	return task;
}

void tst_formdialog::okAnswers()
{
	std::optional<QJsonObject> got;
	bool called = false;
	Task* task = asking(&got, &called);
	QPointer<FormDialog> dlg = new FormDialog(task);

	QCOMPARE(dlg->windowTitle(), QString("Name?"));
	dlg->findChild<QLineEdit*>("name")->setText("Ann");
	dlg->accept();

	QVERIFY(called);
	QCOMPARE(got->value("name").toString(), QString("Ann"));
	QTRY_VERIFY(!dlg); // deletes itself on close
}

void tst_formdialog::invalidAnswerStaysOpen()
{
	std::optional<QJsonObject> got;
	bool called = false;
	Task* task = TaskManager::instance()->create("p.so", "Asks");

	// A list with no row selected answers "", which is not one of its rows.
	task->ask(QJsonDocument::fromJson(R"({ "fields": [ { "type": "list", "key": "l", "rows": [ { "value": "r" } ] } ] })").object(),
		60000, [&](std::optional<QJsonObject> r) { called = true; got = r; });
	FormDialog* dlg = new FormDialog(task);
	dlg->show();

	dlg->findChild<QTreeWidget*>("l")->setCurrentItem(nullptr);
	dlg->accept();

	QVERIFY(!called);
	QVERIFY(dlg->isVisible());
	QVERIFY(task->prompt().has_value());
	dlg->reject();
	QVERIFY(called);
	QVERIFY(!got.has_value());
}

void tst_formdialog::cancelDismisses()
{
	std::optional<QJsonObject> got = QJsonObject();
	bool called = false;
	Task* task = asking(&got, &called);
	FormDialog* dlg = new FormDialog(task);

	dlg->reject();
	QVERIFY(called);
	QVERIFY(!got.has_value());
}

void tst_formdialog::closesWhenAnsweredElsewhere()
{
	std::optional<QJsonObject> got;
	bool called = false;
	Task* task = asking(&got, &called);
	QPointer<FormDialog> dlg = new FormDialog(task);
	dlg->show();

	// As if the web UI answered first.
	task->answer(task->prompt()->id, QJsonDocument::fromJson(R"({ "c": "a", "name": "web" })").object());

	QCOMPARE(got->value("name").toString(), QString("web"));
	QTRY_VERIFY(!dlg);
}

QTEST_MAIN(tst_formdialog)
#include "tst_formdialog.moc"
