/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2010 Lubos Dolezel <lubos a dolezel.info>

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

#include "JSettings.h"
#include "JNativeMethod.h"
#include "JClass.h"
#include "JSignature.h"
#include "JMap.h"
#include "JArray.h"
#include "Settings.h"

void JSettings::registerNatives()
{
	QList<JNativeMethod> natives;

	natives << JNativeMethod("setValue", JSignature().addString().addString(), setValueString);
	natives << JNativeMethod("setValue", JSignature().addString().addLong(), setValueLong);
	natives << JNativeMethod("setValue", JSignature().addString().addBoolean(), setValueBoolean);
	natives << JNativeMethod("setValue", JSignature().addString().addDouble(), setValueDouble);
	natives << JNativeMethod("getValue", JSignature().addString().add("java.lang.Object").ret("java.lang.Object"), getValue);
	natives << JNativeMethod("getValueArray", JSignature().addString().retA("java.lang.Object"), getValueArray);

	JClass("info.dolezel.fatrat.plugins.config.Settings").registerNativeMethods(natives);
}

void JSettings::setValueString(JNIEnv*, jclass, jstring jname, jstring jvalue)
{
	JString name(jname);
	JString value(jvalue);

	setSettingsValue(name.str(), value.str());
}

void JSettings::setValueLong(JNIEnv*, jclass, jstring jname, jlong value)
{
	JString name(jname);
	setSettingsValue(name.str(), qlonglong(value));
}

void JSettings::setValueBoolean(JNIEnv*, jclass, jstring jname, jboolean value)
{
	JString name(jname);
	setSettingsValue(name.str(), value);
}

void JSettings::setValueDouble(JNIEnv*, jclass, jstring jname, jdouble value)
{
	JString name(jname);
	setSettingsValue(name.str(), value);
}

jobject JSettings::getValue(JNIEnv* env, jclass, jstring jname, jobject defValue)
{
	const char* def = "----jplugin-def";
	JString name(jname);
	QVariant v = getSettingsValue(name.str(), def);

	if (v.type() == QVariant::String && v.toString() == def)
		return defValue;

	JObject obj = JMap::nativeToBoxed(v);
	qDebug() << obj.getClass().getClassName();
	return obj.getLocalRef();
}

jarray JSettings::getValueArray(JNIEnv *, jclass, jstring name)
{
	JString id(name);
	QList<QMap<QString, QVariant> > list = getSettingsArray(id.str());

	JArray array = JArray::createObjectArray(list.size(), JClass("java.lang.String"));
	for (int i = 0; i < list.size(); i++)
	{
		JObject obj = JMap::nativeToBoxed(list[i]["item"]);
		array.setObject(i, obj);
	}

	return array.getLocalRef();
}
