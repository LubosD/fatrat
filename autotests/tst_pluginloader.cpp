#include <QJsonDocument>
#include <QTest>

#include "plugins/PluginLoader.h"
#include "plugins/Task.h"
#include "plugins/TransferActions.h"

/* Loading real plugin files: which are kept, which are refused, and that a
 * kept one's actions work. The paths come from CMake. */
static Transfer* fakeTransfer()
{
	static char storage;
	return reinterpret_cast<Transfer*>(&storage);
}

class tst_pluginloader : public QObject
{
	Q_OBJECT
private slots:
	void init();
	void loadsAPluginAndRegistersItsActions();
	void runsAnActionEndToEnd();
	void skipsAGuiPluginWhenHeadless();
	void loadsAGuiPluginWithTheGui();
	void refusesAnotherApiVersion();
	void legacyPluginNeedsTheGui();
	void reportsAMissingFile();
};

void tst_pluginloader::init()
{
	TransferActions::clear();
}

void tst_pluginloader::loadsAPluginAndRegistersItsActions()
{
	PluginLoader::Result r = PluginLoader::load(TESTACTION_PATH, false);

	QCOMPARE(r.outcome, PluginLoader::Loaded);
	QVERIFY(r.info.has_value());
	QCOMPARE(r.info->name, QString("Test action plugin"));
	QVERIFY(TransferActions::find("test.greet").has_value());
	QVERIFY(TransferActions::find("test.count").has_value());
	QVERIFY(TransferActions::find("test.instant").has_value());
	QCOMPARE(TransferActions::find("test.greet")->plugin, QString("testaction.so"));
}

void tst_pluginloader::runsAnActionEndToEnd()
{
	QCOMPARE(PluginLoader::load(TESTACTION_PATH, false).outcome, PluginLoader::Loaded);

	Task* task = TransferActions::trigger(*TransferActions::find("test.greet"), fakeTransfer(), "u1", "file.bin");
	QVERIFY(task);
	QVERIFY(task->prompt().has_value());

	QCOMPARE(task->answer(task->prompt()->id, QJsonDocument::fromJson(R"({ "name": "Ann" })").object()), Task::Answered);
	QCOMPARE(task->state(), Task::Done);
	QCOMPARE(task->message(), QString("Hello Ann"));
}

void tst_pluginloader::skipsAGuiPluginWhenHeadless()
{
	PluginLoader::Result r = PluginLoader::load(TESTACTION_NEEDSGUI_PATH, false);

	QCOMPARE(r.outcome, PluginLoader::SkippedNeedsGui);
	QVERIFY(!r.info.has_value());
	QVERIFY(!TransferActions::find("test.greet").has_value());
}

void tst_pluginloader::loadsAGuiPluginWithTheGui()
{
	QCOMPARE(PluginLoader::load(TESTACTION_NEEDSGUI_PATH, true).outcome, PluginLoader::Loaded);
	QVERIFY(TransferActions::find("test.greet").has_value());
}

void tst_pluginloader::refusesAnotherApiVersion()
{
	PluginLoader::Result r = PluginLoader::load(TESTACTION_WRONGAPI_PATH, true);

	QCOMPARE(r.outcome, PluginLoader::Incompatible);
	QVERIFY(r.message.contains("999"));
	QVERIFY(!TransferActions::find("test.greet").has_value());
}

void tst_pluginloader::legacyPluginNeedsTheGui()
{
	PluginLoader::Result headless = PluginLoader::load(TESTACTION_LEGACY_PATH, false);
	QCOMPARE(headless.outcome, PluginLoader::SkippedNeedsGui);
	QVERIFY(!headless.info.has_value());

	PluginLoader::Result gui = PluginLoader::load(TESTACTION_LEGACY_PATH, true);
	QCOMPARE(gui.outcome, PluginLoader::LoadedLegacy);
	QCOMPARE(gui.info->name, QString("Test legacy plugin"));
}

void tst_pluginloader::reportsAMissingFile()
{
	PluginLoader::Result r = PluginLoader::load("/nonexistent/plugin.so", true);

	QCOMPARE(r.outcome, PluginLoader::Failed);
	QVERIFY(!r.message.isEmpty());
}

QTEST_GUILESS_MAIN(tst_pluginloader)
#include "tst_pluginloader.moc"
