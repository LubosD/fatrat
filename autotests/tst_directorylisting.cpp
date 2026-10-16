#include <QtTest>
#include <unistd.h>
#include "remote/DirectoryListing.h"
#include "remote/RestApi.h"

class TestDirectoryListing : public QObject
{
	Q_OBJECT
private slots:
	void initTestCase();
	void listsSortedSubdirectoriesOnly();
	void hiddenOnlyWhenAsked();
	void emptyPathUsesFallback();
	void missingPathIs404();
	void relativePathIs422();
	void dotDotIsCanonicalised();
	void symlinkIsResolved();
	void unreadableIs403();
	void rootHasNoParent();
private:
	QTemporaryDir m_tmp;
	QString root() const { return QFileInfo(m_tmp.path()).canonicalFilePath(); }
};

static int errorStatus(const QString& path, const QString& fallback = QString())
{
	try { DirectoryListing::list(path, fallback, false); }
	catch (const RestError& e) { return e.status; }
	return 0;
}

static QStringList names(const QJsonObject& o)
{
	QStringList rv;
	for (const QJsonValue& v : o["entries"].toArray())
		rv << v.toObject()["name"].toString();
	return rv;
}

void TestDirectoryListing::initTestCase()
{
	QVERIFY(m_tmp.isValid());
	QDir d(root());
	QVERIFY(d.mkpath("beta"));
	QVERIFY(d.mkpath("Alpha/inner"));
	QVERIFY(d.mkpath("gamma"));
	QVERIFY(d.mkpath(".hidden"));
	QFile f(d.filePath("file.txt"));
	QVERIFY(f.open(QIODevice::WriteOnly));
}

void TestDirectoryListing::listsSortedSubdirectoriesOnly()
{
	QJsonObject o = DirectoryListing::list(root(), QString(), false);
	QCOMPARE(o["path"].toString(), root());
	QCOMPARE(names(o), (QStringList{"Alpha", "beta", "gamma"}));
	QCOMPARE(o["entries"].toArray()[0].toObject()["path"].toString(), root() + "/Alpha");
	QCOMPARE(o["parent"].toString(), QFileInfo(root()).dir().absolutePath());
}

void TestDirectoryListing::hiddenOnlyWhenAsked()
{
	QVERIFY(!names(DirectoryListing::list(root(), QString(), false)).contains(".hidden"));
	QVERIFY(names(DirectoryListing::list(root(), QString(), true)).contains(".hidden"));
}

void TestDirectoryListing::emptyPathUsesFallback()
{
	QCOMPARE(DirectoryListing::list(QString(), root(), false)["path"].toString(), root());
}

void TestDirectoryListing::missingPathIs404()
{
	QCOMPARE(errorStatus(root() + "/nope"), 404);
	QCOMPARE(errorStatus(root() + "/file.txt"), 404);
}

void TestDirectoryListing::relativePathIs422()
{
	QCOMPARE(errorStatus("some/relative"), 422);
}

void TestDirectoryListing::dotDotIsCanonicalised()
{
	QCOMPARE(DirectoryListing::list(root() + "/beta/../gamma", QString(), false)["path"].toString(), root() + "/gamma");
}

void TestDirectoryListing::symlinkIsResolved()
{
	QVERIFY(QFile::link(root() + "/Alpha", root() + "/link"));
	QCOMPARE(DirectoryListing::list(root() + "/link", QString(), false)["path"].toString(), root() + "/Alpha");
	QFile::remove(root() + "/link");
}

void TestDirectoryListing::unreadableIs403()
{
	if (geteuid() == 0)
		QSKIP("root can read everything");
	QDir(root()).mkpath("locked");
	QFile::setPermissions(root() + "/locked", QFileDevice::Permissions());
	QCOMPARE(errorStatus(root() + "/locked"), 403);
	QFile::setPermissions(root() + "/locked", QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
}

void TestDirectoryListing::rootHasNoParent()
{
	QCOMPARE(DirectoryListing::list("/", QString(), false)["parent"].toString(), QString());
}

QTEST_APPLESS_MAIN(TestDirectoryListing)
#include "tst_directorylisting.moc"
