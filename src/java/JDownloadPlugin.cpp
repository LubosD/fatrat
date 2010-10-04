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

template <> QMap<jobject, JObject*> JSingleCObject<JDownloadPlugin>::m_instances = QMap<jobject, JObject*>();
template <> std::auto_ptr<QReadWriteLock> JSingleCObject<JDownloadPlugin>::m_mutex = std::auto_ptr<QReadWriteLock>(new QReadWriteLock);

JDownloadPlugin::JDownloadPlugin(const JClass& cls, const char* sig, JArgs args)
	: JObject(cls, sig, args)
{
	setCObject();
}

JDownloadPlugin::JDownloadPlugin(const char* clsName, const char* sig, JArgs args)
	: JObject(clsName, sig, args)
{
	setCObject();
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
	nm[2].signature = const_cast<char*>("(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
	nm[2].fnPtr = reinterpret_cast<void*>(fetchPage);

	nm[3].name = const_cast<char*>("startDownload");
	nm[3].signature = const_cast<char*>("(Ljava/lang/String;)V");
	nm[3].fnPtr = reinterpret_cast<void*>(startDownload);

	nm[4].name = const_cast<char*>("startWait");
	nm[4].signature = const_cast<char*>("(ILjava/lang/String;)V");
	nm[4].fnPtr = reinterpret_cast<void*>(startWait);

	nm[5].name = const_cast<char*>("logMessage");
	nm[5].signature = const_cast<char*>("(Ljava/lang/String;)V");
	nm[5].fnPtr = reinterpret_cast<void*>(logMessage);

	env->RegisterNatives(cls, nm, 6);
}

void JDownloadPlugin::setMessage(JNIEnv* env, jobject jthis, jstring msg)
{

}

void JDownloadPlugin::setState(JNIEnv* env, jobject jthis, jobject state)
{

}

void JDownloadPlugin::fetchPage(JNIEnv* env, jobject jthis, jstring url, jstring cbMethod, jstring postData)
{

}

void JDownloadPlugin::startDownload(JNIEnv* env, jobject jthis, jstring url)
{

}

void JDownloadPlugin::startWait(JNIEnv* env, jobject jthis, jint seconds, jstring cbMethod)
{

}

void JDownloadPlugin::logMessage(JNIEnv* env, jobject jthis, jstring msg)
{

}


