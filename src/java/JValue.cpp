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

#include "JValue.h"

#include "JVM.h"

JValue::JValue() : m_bHasObject(false) { m_value.j = 0; }

JValue::JValue(const JValue& v) {
  JNIEnv* env = *JVM::instance();
  if ((m_bHasObject = v.m_bHasObject))
    m_value.l = env->NewGlobalRef(v.m_value.l);
  else
    m_value = v.m_value;
}

JValue::JValue(jobject obj) {
  JNIEnv* env = *JVM::instance();
  m_bHasObject = true;
  m_value.l = env->NewGlobalRef(obj);
}

JValue::JValue(bool b) : m_bHasObject(false) { m_value.z = b; }

JValue::JValue(char c) : m_bHasObject(false) { m_value.b = c; }

JValue::JValue(wchar_t c) : m_bHasObject(false) { m_value.c = c; }

JValue::JValue(short s) : m_bHasObject(false) { m_value.s = s; }

JValue::JValue(int i) : m_bHasObject(false) { m_value.i = i; }

JValue::JValue(long long l) : m_bHasObject(false) { m_value.j = l; }

JValue::JValue(float f) : m_bHasObject(false) { m_value.f = f; }

JValue::JValue(double d) : m_bHasObject(false) { m_value.d = d; }

JValue& JValue::operator=(const JValue& v) {
  JNIEnv* env = *JVM::instance();
  if (m_bHasObject) env->DeleteGlobalRef(m_value.l);
  if (v.m_bHasObject)
    m_value.l = env->NewGlobalRef(v.m_value.l);
  else
    m_value = v.m_value;
  return *this;
}

JValue::~JValue() {
  if (m_bHasObject) {
    JNIEnv* env = *JVM::instance();
    env->DeleteGlobalRef(m_value.l);
  }
}
