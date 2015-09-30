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

#include "JNativeMethod.h"
#include <cassert>

JNativeMethod::JNativeMethod()
	: m_function(0)
{
}

void JNativeMethod::setName(QString name)
{
	m_strName = name.toStdString();
}

void JNativeMethod::setSignature(JSignature sig)
{
	m_strSignature = sig.str().toStdString();
}

void JNativeMethod::setSignature(const char* sig)
{
	m_strSignature = sig;
}

JNINativeMethod JNativeMethod::toStruct() const
{
	JNINativeMethod nm;

	assert(!m_strName.empty());
	assert(!m_strSignature.empty());
	assert(m_function != 0);

	nm.name = const_cast<char*>(m_strName.c_str());
	nm.signature = const_cast<char*>(m_strSignature.c_str());
	nm.fnPtr = m_function;

	return nm;
}
