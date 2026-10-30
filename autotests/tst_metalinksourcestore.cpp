#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include "engines/MetalinkSourceStore.h"
#include "RuntimeException.h"

class TestMetalinkSourceStore : public QObject
{
	Q_OBJECT
	QTemporaryDir m_tmp;

	QString write(const QString& name)
	{
		const QString path = m_tmp.filePath(name);
		QFile f(path);
		if (!f.open(QIODevice::WriteOnly))
			qFatal("cannot write");
		f.write("<metalink/>");
		return path;
	}
private slots:
	void keepCopiesUnderUuidKeepingTheName()
	{
		MetalinkSourceStore store(m_tmp.filePath("store"));
		const QString orig = write("ubuntu.meta4");
		const QString copy = store.keep(orig, "{abc}");

		QCOMPARE(QFileInfo(copy).fileName(), QString("ubuntu.meta4"));
		QVERIFY(store.owns(copy));
		QFile::remove(orig);
		QVERIFY(QFile::exists(copy));
	}

	void keepOfMissingFileThrows()
	{
		MetalinkSourceStore store(m_tmp.filePath("store"));
		QVERIFY_THROWS_EXCEPTION(RuntimeException, store.keep(m_tmp.filePath("nope.meta4"), "{x}"));
	}

	void releaseDeletesOnlyOwnCopies()
	{
		MetalinkSourceStore store(m_tmp.filePath("store"));
		const QString copy = store.keep(write("a.metalink"), "{a}");
		const QString foreign = write("b.metalink");

		store.release(foreign);
		QVERIFY(QFile::exists(foreign));

		store.release(copy);
		QVERIFY(!QFile::exists(copy));
		QVERIFY(!QDir(QFileInfo(copy).path()).exists());
	}

	void pruneKeepsWhatIsInUse()
	{
		MetalinkSourceStore store(m_tmp.filePath("store"));
		const QString used = store.keep(write("u.meta4"), "{u}");
		const QString unused = store.keep(write("x.meta4"), "{x}");

		store.prune({ used });
		QVERIFY(QFile::exists(used));
		QVERIFY(!QFile::exists(unused));
	}
};

QTEST_GUILESS_MAIN(TestMetalinkSourceStore)
#include "tst_metalinksourcestore.moc"
