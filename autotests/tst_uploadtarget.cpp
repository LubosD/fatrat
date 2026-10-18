#include <QtTest>
#include "engines/UploadTarget.h"

class UploadTargetTest : public QObject
{
	Q_OBJECT
private slots:
	void targetUserInfoWins()
	{
		QUrl cur("ftp://old:oldpw@host/dir/");
		QUrl u = composeUploadTarget(cur, QJsonObject{{"target", "ftp://bob:pw@other/x/"}});
		QCOMPARE(u.userName(), QString("bob"));
		QCOMPARE(u.password(), QString("pw"));
		QCOMPARE(u.host(), QString("other"));
	}
	void explicitKeysBeatTargetUserInfo()
	{
		QUrl cur("ftp://old:oldpw@host/dir/");
		QUrl u = composeUploadTarget(cur, QJsonObject{{"target", "ftp://bob:pw@other/x/"}, {"username", "alice"}});
		QCOMPARE(u.userName(), QString("alice"));
		QCOMPARE(u.password(), QString("pw"));
		u = composeUploadTarget(cur, QJsonObject{{"target", "ftp://bob:pw@other/x/"}, {"password", "zz"}});
		QCOMPARE(u.userName(), QString("bob"));
		QCOMPARE(u.password(), QString("zz"));
	}
	void targetWithoutUserInfoKeepsCredentials()
	{
		QUrl cur("ftp://old:oldpw@host/dir/");
		QUrl u = composeUploadTarget(cur, QJsonObject{{"target", "ftp://other/x/"}});
		QCOMPARE(u.userName(), QString("old"));
		QCOMPARE(u.password(), QString("oldpw"));
		QCOMPARE(u.host(), QString("other"));
	}
	void onlyPasswordChanged()
	{
		QUrl cur("ftp://old:oldpw@host/dir/");
		QUrl u = composeUploadTarget(cur, QJsonObject{{"password", "new"}});
		QCOMPARE(u.userName(), QString("old"));
		QCOMPARE(u.password(), QString("new"));
		QCOMPARE(u.host(), QString("host"));
	}
};

QTEST_GUILESS_MAIN(UploadTargetTest)
#include "tst_uploadtarget.moc"
