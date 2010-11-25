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


#ifndef JNATIVEMETHOD_H
#define JNATIVEMETHOD_H

#include "config.h"
#ifndef WITH_JPLUGINS
#	error This file is not supposed to be included!
#endif
#include <jni.h>
#include <QString>
#include "JSignature.h"

class JNativeMethod
{
public:
	JNativeMethod();
	template<typename T> JNativeMethod(QString name, JSignature sig, T* func)
	{
		setName(name);
		setSignature(sig);
		setNativeFunction(func);
	}

	void setName(QString);
	void setSignature(JSignature sig);
	void setSignature(const char* sig);
	template<typename T> void setNativeFunction(T* func)
	{
		m_function = reinterpret_cast<void*>(func);
	}

	JNINativeMethod toStruct() const;
private:
	std::string m_strSignature, m_strName;
	void* m_function;
};

#endif // JNATIVEMETHOD_H
