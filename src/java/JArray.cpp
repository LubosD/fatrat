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

#include "JArray.h"
#include "JVM.h"
#include "JScope.h"
#include "RuntimeException.h"

JArray::JArray()
	: JObject()
{
}

JArray::JArray(jarray arr)
	: JObject(arr)
{
}

JArray::JArray(jobject arr)
	: JObject(arr)
{
}

JArray::JArray(const char* type, size_t length)
{
	JScope s;
	JNIEnv* env = *JVM::instance();

	switch (*type)
	{
	case 'Z':
		m_object = env->NewBooleanArray(length);
		break;
	case 'I':
		m_object = env->NewIntArray(length);
		break;
	case 'S':
		m_object = env->NewShortArray(length);
		break;
	case 'L':
		m_object = env->NewObjectArray(length, JClass(type+1), 0);
		break;
	case 'J':
		m_object = env->NewLongArray(length);
		break;
	case 'C':
		m_object = env->NewCharArray(length);
		break;
	case 'B':
		m_object = env->NewByteArray(length);
		break;
	case 'F':
		m_object = env->NewFloatArray(length);
		break;
	case 'D':
		m_object = env->NewDoubleArray(length);
		break;
	default:
		throw RuntimeException(QObject::tr("Unknown type ID: %1").arg(type));
	}
}

size_t JArray::size() const
{
	JNIEnv* env = *JVM::instance();
	return env->GetArrayLength(jarray(m_object));
}

int JArray::getInt(int index) const
{
	JNIEnv* env = *JVM::instance();
	jint v;
	env->GetIntArrayRegion(jintArray(m_object), index, 1, &v);
	return v;
}

void JArray::setInt(int index, int value)
{
	JNIEnv* env = *JVM::instance();
	jint v = value;
	env->SetIntArrayRegion(jintArray(m_object), index, 1, &v);
}

qlonglong JArray::getLongLong(int index) const
{
	JNIEnv* env = *JVM::instance();
	jlong v;
	env->GetLongArrayRegion(jlongArray(m_object), index, 1, &v);
	return qlonglong(v);
}

void JArray::setLongLong(int index, qlonglong value)
{
	JNIEnv* env = *JVM::instance();
	jlong v = value;
	env->SetLongArrayRegion(jlongArray(m_object), index, 1, &v);
}

short JArray::getShort(int index) const
{
	JNIEnv* env = *JVM::instance();
	jshort v;
	env->GetShortArrayRegion(jshortArray(m_object), index, 1, &v);
	return v;
}

void JArray::setShort(int index, short value)
{
	JNIEnv* env = *JVM::instance();
	jshort v = value;
	env->SetShortArrayRegion(jshortArray(m_object), index, 1, &v);
}

wchar_t JArray::getWchar_t(int index) const
{
	JNIEnv* env = *JVM::instance();
	jchar v;
	env->GetCharArrayRegion(jcharArray(m_object), index, 1, &v);
	return v;
}

void JArray::setWchar_t(int index, wchar_t value)
{
	JNIEnv* env = *JVM::instance();
	jchar v = value;
	env->SetCharArrayRegion(jcharArray(m_object), index, 1, &v);
}

char JArray::getByte(int index) const
{
	JNIEnv* env = *JVM::instance();
	jbyte v;
	env->GetByteArrayRegion(jbyteArray(m_object), index, 1, &v);
	return v;
}

void JArray::setByte(int index, char value)
{
	JNIEnv* env = *JVM::instance();
	jbyte v = value;
	env->SetByteArrayRegion(jbyteArray(m_object), index, 1, &v);
}

float JArray::getFloat(int index) const
{
	JNIEnv* env = *JVM::instance();
	jfloat v;
	env->GetFloatArrayRegion(jfloatArray(m_object), index, 1, &v);
	return v;
}

void JArray::setFloat(int index, float value)
{
	JNIEnv* env = *JVM::instance();
	jfloat v = value;
	env->SetFloatArrayRegion(jfloatArray(m_object), index, 1, &v);
}

double JArray::getDouble(int index) const
{
	JNIEnv* env = *JVM::instance();
	jdouble v;
	env->GetDoubleArrayRegion(jdoubleArray(m_object), index, 1, &v);
	return v;
}

void JArray::setDouble(int index, double value)
{
	JNIEnv* env = *JVM::instance();
	jdouble v = value;
	env->SetDoubleArrayRegion(jdoubleArray(m_object), index, 1, &v);
}

bool JArray::getBool(int index) const
{
	JNIEnv* env = *JVM::instance();
	jboolean v;
	env->GetBooleanArrayRegion(jbooleanArray(m_object), index, 1, &v);
	return v;
}

void JArray::setBool(int index, bool value)
{
	JNIEnv* env = *JVM::instance();
	jboolean v = value;
	env->SetBooleanArrayRegion(jbooleanArray(m_object), index, 1, &v);
}

JObject JArray::getObject(int index) const
{
	JNIEnv* env = *JVM::instance();
	return env->GetObjectArrayElement(jobjectArray(m_object), index);
}

void JArray::setObject(int index, JObject value)
{
	JNIEnv* env = *JVM::instance();
	env->SetObjectArrayElement(jobjectArray(m_object), index, value);
}

