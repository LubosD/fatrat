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

#include "JString.h"
#include <jni.h>
#include "JVM.h"
#include "JScope.h"
#include <QtDebug>

JString::JString()
{
	JNIEnv* env = *JVM::instance();
	jstring str = env->NewString(0, 0);
	m_object = env->NewGlobalRef(str);
	env->DeleteLocalRef(str);
}

JString::JString(const JString& str)
{
	JNIEnv* env = *JVM::instance();
	m_object = env->NewGlobalRef(str.m_object);
}

#ifdef WITH_CXX0X
JString::JString(JString&& str)
{
	m_object = str.m_object;
	str.m_object = 0;
}
#endif

JString::JString(const QString& str)
{
	*this = str.toUtf8();
}

JString::JString(const QByteArray& str)
{
	*this = str.constData();
}

JString::JString(jstring str)
{
	JNIEnv* env = *JVM::instance();
	m_object = env->NewGlobalRef(str);
}

JString::JString(const char* str)
{
	JNIEnv* env = *JVM::instance();
	jstring jstr = env->NewStringUTF(str);
	m_object = env->NewGlobalRef(jstr);
	env->DeleteLocalRef(jstr);
}

JString& JString::operator=(const QString& str)
{
	return (*this) = str.toUtf8();
}

JString& JString::operator=(const QByteArray& str)
{
	return (*this) = str.constData();
}

JString& JString::operator=(const char* str)
{
	JNIEnv* env = *JVM::instance();
	if (m_object)
		env->DeleteGlobalRef(m_object);
	jstring jstr = env->NewStringUTF(str);
	m_object = env->NewGlobalRef(jstr);
	env->DeleteLocalRef(jstr);

	return *this;
}

JString& JString::operator=(const JString& str)
{
	JNIEnv* env = *JVM::instance();
	if (m_object)
		env->DeleteGlobalRef(m_object);
	m_object = env->NewGlobalRef(str.m_object);

	return *this;
}

#ifdef WITH_CXX0X
JString& JString::operator=(JString&& str)
{
	JNIEnv* env = *JVM::instance();
	if (m_object)
		env->DeleteGlobalRef(m_object);
	m_object = str.m_object;
	str.m_object = 0;
	return *this;
}
#endif

JString::operator jstring() const
{
	return (jstring) m_object;
}

QString JString::str() const
{
	if (isNull())
	{
		qDebug() << "WARNING: JString::str() on a null string";
		return QString();
	}

	QString val;
	JNIEnv* env = *JVM::instance();

	const char* chars = env->GetStringUTFChars(jstring(m_object), 0);

	val = QString::fromUtf8(chars);

	env->ReleaseStringUTFChars(jstring(m_object), chars);
	return val;
}

JString::operator QString() const
{
	return str();
}

JString::operator QByteArray() const
{
	QByteArray val;
	JNIEnv* env = *JVM::instance();
	const char* chars = env->GetStringUTFChars(jstring(m_object), 0);

	val = chars;

	env->ReleaseStringUTFChars(jstring(m_object), chars);
	return val;
}

size_t JString::size() const
{
	JNIEnv* env = *JVM::instance();
	return env->GetStringLength(jstring(m_object));
}

