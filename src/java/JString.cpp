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
	JScope s;
	JNIEnv* env = *JVM::instance();
	m_object = env->NewString(0, 0);
	m_ref = env->NewGlobalRef(m_object);
}

JString::JString(const JString& str)
{
	JScope s;
	JNIEnv* env = *JVM::instance();
	m_object = str.m_object;
	m_ref = env->NewGlobalRef(m_object);
}

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
	m_object = str;
	m_ref = env->NewGlobalRef(m_object);
}

JString::JString(const char* str)
{
	JScope s;
	JNIEnv* env = *JVM::instance();
	m_object = env->NewStringUTF(str);
	m_ref = env->NewGlobalRef(m_object);
}

void JString::operator=(const QString& str)
{
	(*this) = str.toUtf8();
}

void JString::operator=(const QByteArray& str)
{
	(*this) = str.constData();
}

void JString::operator=(const char* str)
{
	JScope s;
	JNIEnv* env = *JVM::instance();
	env->DeleteGlobalRef(m_ref);
	m_object = env->NewStringUTF(str);
	m_ref = env->NewGlobalRef(m_object);
}

void JString::operator=(const JString& str)
{
	JScope s;
	JNIEnv* env = *JVM::instance();
	env->DeleteGlobalRef(m_ref);
	m_object = str.m_object;
	m_ref = env->NewGlobalRef(m_object);
}

JString::operator jstring() const
{
	return (jstring) m_object;
}

QString JString::str() const
{
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

