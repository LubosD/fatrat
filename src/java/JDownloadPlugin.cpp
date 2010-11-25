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
#include "engines/JavaDownload.h"
#include "captcha/Captcha.h"
#include <cassert>

template <> QMap<jobject, JObject*> JSingleCObject<JDownloadPlugin>::m_instances = QMap<jobject, JObject*>();
template <> std::auto_ptr<QReadWriteLock> JSingleCObject<JDownloadPlugin>::m_mutex = std::auto_ptr<QReadWriteLock>(new QReadWriteLock);

QMap<QString,JObject> JDownloadPlugin::m_captchaCallbacks;

JDownloadPlugin::JDownloadPlugin(const JClass& cls, const char* sig, JArgs args)
	: JObject(cls, sig, args), m_transfer(0)
{
	setCObject();
	m_network = new QNetworkAccessManager(this);
	connect(m_network, SIGNAL(finished(QNetworkReply*)), this, SLOT(fetchFinished(QNetworkReply*)));

	connect(&m_timer, SIGNAL(timeout()), this, SLOT(secondElapsed()));
}

JDownloadPlugin::JDownloadPlugin(const char* clsName, const char* sig, JArgs args)
	: JObject(clsName, sig, args), m_transfer(0)
{
	setCObject();
	m_network = new QNetworkAccessManager(this);
	connect(m_network, SIGNAL(finished(QNetworkReply*)), this, SLOT(fetchFinished(QNetworkReply*)));

	connect(&m_timer, SIGNAL(timeout()), this, SLOT(secondElapsed()));
}

void JDownloadPlugin::registerNatives()
{
	QList<JNativeMethod> natives;

	natives << JNativeMethod("setMessage", JSignature().addString(), setMessage);
	natives << JNativeMethod("setState", JSignature().add("info.dolezel.fatrat.plugins.DownloadPlugin$State"), setState);
	natives << JNativeMethod("fetchPage", JSignature().addString().add("info.dolezel.fatrat.plugins.listeners.PageFetchListener").addString(), fetchPage);
	natives << JNativeMethod("startDownload", JSignature().addString(), startDownload);
	natives << JNativeMethod("startWait", JSignature().addInt().add("info.dolezel.fatrat.plugins.listeners.WaitListener"), startWait);
	natives << JNativeMethod("logMessage", JSignature().addString(), logMessage);
	natives << JNativeMethod("solveCaptcha", JSignature().addString().add("info.dolezel.fatrat.plugins.listeners.CaptchaListener"), solveCaptcha);

	JClass("info.dolezel.fatrat.plugins.DownloadPlugin").registerNativeMethods(natives);
}

void JDownloadPlugin::captchaSolved(QString url, QString solution)
{
	assert(m_captchaCallbacks.contains(url));

	JObject& obj = m_captchaCallbacks[url];
	if (!solution.isEmpty())
		obj.call("onSolved", JSignature().addString(), JArgs() << solution);
	else
		obj.call("onFailed");
}

void JDownloadPlugin::solveCaptcha(JNIEnv* env, jobject jthis, jstring jurl, jobject cb)
{
	QString url = JString(jurl);
	m_captchaCallbacks[url] = JObject(cb);

	Captcha::processCaptcha(url, captchaSolved);
}

void JDownloadPlugin::setMessage(JNIEnv* env, jobject jthis, jstring msg)
{
	getCObject(jthis)->m_transfer->setMessage(JString(msg));
}

void JDownloadPlugin::setState(JNIEnv* env, jobject jthis, jobject state)
{
	JStateEnum e(state);
	getCObject(jthis)->m_transfer->setState(e.value());
}

void JDownloadPlugin::fetchPage(JNIEnv* env, jobject jthis, jstring jurl, jobject cbInterface, jstring postData)
{
	JDownloadPlugin* This = getCObject(jthis);
	QString url = JString(jurl).toString();
	QNetworkReply* reply;

	if (!postData)
		reply = This->m_network->get(QNetworkRequest(url));
	else
	{
		QByteArray pd = JString(postData).toString().toUtf8();
		reply = This->m_network->post(QNetworkRequest(url), pd);
	}

	This->m_fetchCallbacks[reply] = cbInterface;
}

void JDownloadPlugin::startDownload(JNIEnv* env, jobject jthis, jstring url)
{
	JDownloadPlugin* This = getCObject(jthis);
	This->m_transfer->startDownload(JString(url));
}

void JDownloadPlugin::startWait(JNIEnv* env, jobject jthis, jint seconds, jobject cbInterface)
{
	JDownloadPlugin* This = getCObject(jthis);

	This->m_nSecondsLeft = seconds;
	This->m_waitCallback = cbInterface;
	This->m_timer.start(1000);
}

void JDownloadPlugin::fetchFinished(QNetworkReply* reply)
{
	reply->deleteLater();

	JObject iface = m_fetchCallbacks[reply];
	if (reply->error() != QNetworkReply::NoError)
	{
		iface.call("onFailed", JSignature().addString(), JArgs() << reply->errorString());
	}
	else
	{
		JByteBuffer buf;
		qint64 bytes = reply->bytesAvailable();
		void* mem = buf.allocate(bytes);

		reply->read(static_cast<char*>(mem), bytes);

		iface.call("onCompleted", JSignature().add("java.nio.ByteBuffer"), JArgs() << buf);
	}
	m_fetchCallbacks.remove(reply);
}

void JDownloadPlugin::secondElapsed()
{
	m_waitCallback.call("onSecondElapsed", JSignature().addInt(), JArgs() << m_nSecondsLeft);

	if (!(--m_nSecondsLeft))
	{
		m_timer.stop();
		m_waitCallback.setNull();
	}
}

void JDownloadPlugin::logMessage(JNIEnv* env, jobject jthis, jstring msg)
{
	getCObject(jthis)->m_transfer->logMessage(JString(msg));
}

JDownloadPlugin::JStateEnum::JStateEnum(jobject obj)
	: JObject(obj)
{
}

Transfer::State JDownloadPlugin::JStateEnum::value() const
{
	int state = const_cast<JStateEnum*>(this)->call("value", JSignature().retInt()).toInt();
	return Transfer::State( state );
}

