#include <QJsonDocument>
#include <QTest>
#include <dlfcn.h>

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
	void refusesAPluginWithoutInit();
	void refusesAPluginWithoutRequiresGui();
	void aThrowingInitLeavesNothingBehind();
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
	QVERIFY(!dlopen(TESTACTION_WRONGAPI_PATH, RTLD_NOW | RTLD_NOLOAD));
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

void tst_pluginloader::refusesAPluginWithoutInit()
{
	PluginLoader::Result r = PluginLoader::load(TESTACTION_NOINIT_PATH, true);

	QCOMPARE(r.outcome, PluginLoader::Incompatible);
	QVERIFY(r.message.contains("initPlugin"));
	QVERIFY(!dlopen(TESTACTION_NOINIT_PATH, RTLD_NOW | RTLD_NOLOAD));
}

void tst_pluginloader::refusesAPluginWithoutRequiresGui()
{
	PluginLoader::Result r = PluginLoader::load(TESTACTION_NOREQUIRES_PATH, true);

	QCOMPARE(r.outcome, PluginLoader::Incompatible);
	QVERIFY(r.message.contains("requiresGui"));
	QVERIFY(!TransferActions::find("test.greet").has_value());
	QVERIFY(!dlopen(TESTACTION_NOREQUIRES_PATH, RTLD_NOW | RTLD_NOLOAD));
}

void tst_pluginloader::aThrowingInitLeavesNothingBehind()
{
	PluginLoader::Result r = PluginLoader::load(TESTACTION_THROWS_PATH, true);

	QCOMPARE(r.outcome, PluginLoader::Failed);
	QVERIFY(r.message.contains("on purpose"));
	QVERIFY(!r.info.has_value());
	// It registered test.greet before throwing; that must be gone again.
	QVERIFY(!TransferActions::find("test.greet").has_value());

	// And the loader must not still be attributing registrations to the plugin.
	TransferAction a;
	a.id = "later.action";
	a.trigger = [](Transfer*, TaskHost&) {};
	registerTransferAction(a);
	QCOMPARE(TransferActions::find("later.action")->plugin, QString("fatrat"));
}

void tst_pluginloader::reportsAMissingFile()
{
	PluginLoader::Result r = PluginLoader::load("/nonexistent/plugin.so", true);

	QCOMPARE(r.outcome, PluginLoader::Failed);
	QVERIFY(!r.message.isEmpty());
}

QTEST_GUILESS_MAIN(tst_pluginloader)
#include "tst_pluginloader.moc"
