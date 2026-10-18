/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2026 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 3 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/


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
