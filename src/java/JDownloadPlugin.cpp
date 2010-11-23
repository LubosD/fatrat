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

template <> QMap<jobject, JObject*> JSingleCObject<JDownloadPlugin>::m_instances = QMap<jobject, JObject*>();
template <> std::auto_ptr<QReadWriteLock> JSingleCObject<JDownloadPlugin>::m_mutex = std::auto_ptr<QReadWriteLock>(new QReadWriteLock);

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
	JNIEnv* env = *JVM::instance();
	jclass cls = env->FindClass("info/dolezel/fatrat/plugins/DownloadPlugin");
	if (!cls)
	{
		Logger::global()->enterLogMessage("JPlugin", QObject::tr("Failed to register native methods"));
		return;
	}

	JNINativeMethod nm[6];

	nm[0].name = const_cast<char*>("setMessage");
	nm[0].signature = const_cast<char*>("(Ljava/lang/String;)V");
	nm[0].fnPtr = reinterpret_cast<void*>(setMessage);

	nm[1].name = const_cast<char*>("setState");
	nm[1].signature = const_cast<char*>("(Linfo/dolezel/fatrat/plugins/DownloadPlugin$State;)V");
	nm[1].fnPtr = reinterpret_cast<void*>(setState);

	nm[2].name = const_cast<char*>("fetchPage");
	nm[2].signature = const_cast<char*>("(Ljava/lang/String;Linfo/dolezel/fatrat/plugins/listeners/PageFetchListener;Ljava/lang/String;)V");
	nm[2].fnPtr = reinterpret_cast<void*>(fetchPage);

	nm[3].name = const_cast<char*>("startDownload");
	nm[3].signature = const_cast<char*>("(Ljava/lang/String;)V");
	nm[3].fnPtr = reinterpret_cast<void*>(startDownload);

	nm[4].name = const_cast<char*>("startWait");
	nm[4].signature = const_cast<char*>("(ILinfo/dolezel/fatrat/plugins/listeners/WaitListener;)V");
	nm[4].fnPtr = reinterpret_cast<void*>(startWait);

	nm[5].name = const_cast<char*>("logMessage");
	nm[5].signature = const_cast<char*>("(Ljava/lang/String;)V");
	nm[5].fnPtr = reinterpret_cast<void*>(logMessage);

	env->RegisterNatives(cls, nm, 6);
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

