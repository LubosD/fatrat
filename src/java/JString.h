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

#ifndef JSTRING_H
#define JSTRING_H

#include "config.h"
#ifndef WITH_JPLUGINS
#	error This file is not supposed to be included!
#endif
#include <jni.h>
#include <QString>
#include <QByteArray>
#include "JObject.h"

class JString : public JObject
{
public:
	JString();
	JString(jstring str);
	JString(const char* str);
	JString(const QString& str);
	JString(const QByteArray& str);
	JString(const JString& str);
#ifdef WITH_CXX0X
	JString(JString&& str);
#endif

	JString& operator=(const char* str);
	JString& operator=(const QString& str);
	JString& operator=(const QByteArray& str);
	JString& operator=(const JString& str);
#ifdef WITH_CXX0X
	JString& operator=(JString&& str);
#endif

	operator jstring() const;
	operator QString() const;
	operator QByteArray() const;

	QString str() const;
	size_t size() const;
	size_t length() const { return size(); }
};

#endif // JSTRING_H
