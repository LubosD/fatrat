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

#include "JDownloadPlugin.h"
#include "Logger.h"
#include "JVM.h"
#include "JString.h"
#include "JByteBuffer.h"
#include "JMap.h"
#include "JException.h"
#include "engines/JavaDownload.h"
#include "captcha/Captcha.h"
#include <QBuffer>
#include <cassert>

QMap<QString,JDownloadPlugin::RequesterReceiver> JDownloadPlugin::m_captchaCallbacks;

JDownloadPlugin::JDownloadPlugin(const JClass& cls, const char* sig, JArgs args)
	: JTransferPlugin(cls, sig, args)
{
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(secondElapsed()));
}

JDownloadPlugin::JDownloadPlugin(const char* clsName, const char* sig, JArgs args)
	: JTransferPlugin(clsName, sig, args)
{
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(secondElapsed()));
}

void JDownloadPlugin::registerNatives()
{
	QList<JNativeMethod> natives;

	natives << JNativeMethod("startDownload", JSignature().addString().addString().addString().addString(), startDownload);
	natives << JNativeMethod("startWait", JSignature().addInt().add("info.dolezel.fatrat.plugins.listeners.WaitListener"), startWait);
	natives << JNativeMethod("solveCaptcha", JSignature().addString().add("info.dolezel.fatrat.plugins.listeners.CaptchaListener"), solveCaptcha);
	natives << JNativeMethod("reportFileName", JSignature().addString(), reportFileName);

	JClass("info.dolezel.fatrat.plugins.DownloadPlugin").registerNativeMethods(natives);
}

void JDownloadPlugin::captchaSolved(QString url, QString solution)
{
	assert(m_captchaCallbacks.contains(url));
	JDownloadPlugin* This = static_cast<JDownloadPlugin*>(m_captchaCallbacks[url].first);

	// Needs to be called from the right thread
	QMetaObject::invokeMethod(This, "captchaSolvedSlot", Qt::QueuedConnection, Q_ARG(QString, url), Q_ARG(QString, solution));
}

void JDownloadPlugin::captchaSolvedSlot(QString url, QString solution)
{
	assert(m_captchaCallbacks.contains(url));

	JObject& obj = m_captchaCallbacks[url].second;

	try
	{
		if (!solution.isEmpty())
		{
			m_transfer->enterLogMessage(QLatin1String("JDownloadPlugin::captchaSolved(): ")+url+" -> "+solution);
			obj.call("onSolved", JSignature().addString(), solution);
		}
		else
		{
			m_transfer->enterLogMessage(QLatin1String("JDownloadPlugin::captchaSolved(): ")+url+" -> ?");
			obj.call("onFailed");
		}
	}
	catch (const JException& e)
	{
		m_transfer->setMessage(tr("Java exception: %1").arg(e.what()));
		m_transfer->setState(Transfer::Failed);
	}

	m_captchaCallbacks.remove(url);
}

void JDownloadPlugin::solveCaptcha(JNIEnv* env, jobject jthis, jstring jurl, jobject cb)
{
	JDownloadPlugin* This = static_cast<JDownloadPlugin*>(getCObject(jthis));
	QString url = JString(jurl);

	This->m_transfer->enterLogMessage(QLatin1String("JDownloadPlugin::solveCaptcha(): ")+url);
	m_captchaCallbacks[url] = RequesterReceiver(getCObject(jthis), cb);

	Captcha::processCaptcha(url, captchaSolved);
}

void JDownloadPlugin::reportFileName(JNIEnv* env, jobject jthis, jstring jname)
{
	JDownloadPlugin* This = static_cast<JDownloadPlugin*>(getCObject(jthis));
	JString name(jname);

	static_cast<JavaDownload*>(This->transfer())->setName(name.str());
	This->m_transfer->enterLogMessage(QLatin1String("JDownloadPlugin::reportFileName(): ")+name.str());
}


void JDownloadPlugin::startDownload(JNIEnv* env, jobject jthis, jstring url, jstring referrer, jstring userAgent, jstring fileName)
{
	JDownloadPlugin* This = static_cast<JDownloadPlugin*>(getCObject(jthis));
	QString str = JString(url).str();
	This->m_transfer->enterLogMessage(QLatin1String("startDownload(): ")+str);

	QString ref, ua;
	if (referrer)
		ref = JString(referrer).str();
	if (userAgent)
		ua = JString(userAgent).str();
	if (fileName)
		str += "#__filename="+JString(fileName).str().replace('/', '-');

	QList<QNetworkCookie> c = This->m_network->cookieJar()->cookiesForUrl(str);
	static_cast<JavaDownload*>(This->m_transfer)->startDownload(str, c, ref, ua);
}

QMap<QString,QString> JDownloadPlugin::cookiesToMap(const QList<QNetworkCookie>& list)
{
	QMap<QString,QString> rv;
	foreach (QNetworkCookie c, list)
		rv[c.name()] = c.value();
	return rv;
}

void JDownloadPlugin::startWait(JNIEnv* env, jobject jthis, jint seconds, jobject cbInterface)
{
	JDownloadPlugin* This = static_cast<JDownloadPlugin*>(getCObject(jthis));
	This->m_transfer->enterLogMessage(QString("JDownloadPlugin::startWait(): %1").arg(seconds));

	This->m_nSecondsLeft = seconds;
	This->m_waitCallback = cbInterface;
	This->m_timer.start(1000);
}


void JDownloadPlugin::secondElapsed()
{
	m_nSecondsLeft--;
	m_waitCallback.call("onSecondElapsed", JSignature().addInt(), m_nSecondsLeft);

	if (!m_nSecondsLeft)
	{
		m_timer.stop();
		m_waitCallback.setNull();
	}
}

void JDownloadPlugin::abort()
{
	JTransferPlugin::abort();

	// prune m_captchaCallbacks and m_fetchCallbacks
	for(QMap<QString,RequesterReceiver>::iterator it = m_captchaCallbacks.begin(); it != m_captchaCallbacks.end();)
	{
		if (it.value().first == this)
		{
			qDebug() << "Aborting captcha for" << it.key();
			Captcha::abortCaptcha(it.key());
			it = m_captchaCallbacks.erase(it);
		}
		else
			it++;
	}

	m_timer.stop();
	m_waitCallback.setNull();
}
