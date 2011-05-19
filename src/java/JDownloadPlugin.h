/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2010 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/


#ifndef JDOWNLOADPLUGIN_H
#define JDOWNLOADPLUGIN_H

#include "config.h"
#ifndef WITH_JPLUGINS
#	error This file is not supposed to be included!
#endif

#include "JTransferPlugin.h"
#include "Transfer.h"
#include <QTimer>
#include <QMap>
#include <QPair>

class JavaDownload;

class JDownloadPlugin : public JTransferPlugin
{
Q_OBJECT
public:
	JDownloadPlugin(const JClass& cls, const char* sig = "()V", JArgs args = JArgs());
	JDownloadPlugin(const char* clsName, const char* sig = "()V", JArgs args = JArgs());

	void abort();

	static void registerNatives();

	// JNI methods
	static void startDownload(JNIEnv *, jobject, jstring, jstring, jstring, jstring);
	static void startWait(JNIEnv *, jobject, jint, jobject);
	static void solveCaptcha(JNIEnv *, jobject, jstring, jobject);
	static void reportFileName(JNIEnv *, jobject, jstring);

	static void captchaSolved(QString url, QString solution);
private slots:
	void secondElapsed();
	void captchaSolvedSlot(QString url, QString solution);
protected:
	static QMap<QString,QString> cookiesToMap(const QList<QNetworkCookie>& list);
private:
	QTimer m_timer;
	static QMap<QString,RequesterReceiver> m_captchaCallbacks;
	JObject m_waitCallback;
	int m_nSecondsLeft;
};

#endif // JDOWNLOADPLUGIN_H
