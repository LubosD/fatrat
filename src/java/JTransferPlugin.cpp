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


#include "JTransferPlugin.h"
#include "engines/JavaDownload.h"
#include "JString.h"
#include "JMap.h"
#include "JException.h"
#include "engines/StaticTransferMessage.h"

JTransferPlugin::JTransferPlugin(const JClass& cls, const char* sig, JArgs args)
	: JPlugin(cls, sig, args)
{
}

JTransferPlugin::JTransferPlugin(const char* clsName, const char* sig, JArgs args)
		: JPlugin(clsName, sig, args)
{
}

void JTransferPlugin::abort()
{
	JPlugin::abort();
}

void JTransferPlugin::registerNatives()
{
	QList<JNativeMethod> natives;

	natives << JNativeMethod("setMessage", JSignature().addString(), setMessage);
	natives << JNativeMethod("setState", JSignature().add("info.dolezel.fatrat.plugins.TransferPlugin$State"), setState);
	natives << JNativeMethod("logMessage", JSignature().addString(), logMessage);

	JClass("info.dolezel.fatrat.plugins.TransferPlugin").registerNativeMethods(natives);
}

void JTransferPlugin::setMessage(JNIEnv* env, jobject jthis, jstring msg)
{
	JPlugin* plugin = getCObject(jthis);
	plugin->transfer()->setMessage(JString(msg));
}

void JTransferPlugin::setState(JNIEnv* env, jobject jthis, jobject state)
{
	JStateEnum e(state);
	getCObject(jthis)->transfer()->setState(e.value());
}



void JTransferPlugin::logMessage(JNIEnv* env, jobject jthis, jstring msg)
{
	getCObject(jthis)->transfer()->enterLogMessage(JString(msg));
}

JTransferPlugin::JStateEnum::JStateEnum(jobject obj)
	: JObject(obj)
{
}

Transfer::State JTransferPlugin::JStateEnum::value() const
{
	int state = const_cast<JStateEnum*>(this)->call("value", JSignature().retInt()).toInt();
	return Transfer::State( state );
}
