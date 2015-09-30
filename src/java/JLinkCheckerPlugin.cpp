/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2012 Lubos Dolezel <lubos a dolezel.info>

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

#include "JLinkCheckerPlugin.h"
#include "JArray.h"
#include "JString.h"

JLinkCheckerPlugin::JLinkCheckerPlugin(const JClass& cls, const char* sig, JArgs args)
	: JPlugin(cls, sig, args)
{
}

JLinkCheckerPlugin::JLinkCheckerPlugin(const char* clsName, const char* sig, JArgs args)
	: JPlugin(clsName, sig, args)
{
}

void JLinkCheckerPlugin::registerNatives()
{
	QList<JNativeMethod> natives;

	natives << JNativeMethod("reportBroken", JSignature().addStringA(), reportBroken);
	natives << JNativeMethod("reportWorking", JSignature().addStringA(), reportWorking);
	natives << JNativeMethod("reportDone", JSignature(), reportDone);

	JClass("info.dolezel.fatrat.plugins.LinkCheckerPlugin").registerNativeMethods(natives);
}

void JLinkCheckerPlugin::reportBroken(JNIEnv*, jobject jthis, jarray jlinks)
{
	JLinkCheckerPlugin* This = static_cast<JLinkCheckerPlugin*>(getCObject(jthis));
	JArray arr(jlinks);
	QStringList broken;

	for (size_t i = 0; i < arr.size(); i++)
		broken << arr.getObject(i).toStringShallow().str();

	emit This->reportedBroken(broken);
}

void JLinkCheckerPlugin::reportWorking(JNIEnv*, jobject jthis, jarray jlinks)
{
	JLinkCheckerPlugin* This = static_cast<JLinkCheckerPlugin*>(getCObject(jthis));
	JArray arr(jlinks);
	QStringList working;

	for (size_t i = 0; i < arr.size(); i++)
		working << arr.getObject(i).toStringShallow().str();

	emit This->reportedWorking(working);
}

void JLinkCheckerPlugin::reportDone(JNIEnv*, jobject jthis)
{
	JLinkCheckerPlugin* This = static_cast<JLinkCheckerPlugin*>(getCObject(jthis));
	emit This->done();
}
