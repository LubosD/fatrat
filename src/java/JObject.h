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

#ifndef JOBJECT_H
#define JOBJECT_H

#include "config.h"
#ifndef WITH_JPLUGINS
#	error This file is not supposed to be included!
#endif
#include <jni.h>
#include <QObject>
#include <QMetaType>
#include <QVariant>
#include "JClass.h"

class JString;
class JArray;

class JObject
{
public:
	JObject();
	JObject(jobject obj);
	JObject(const JObject& obj);
	JObject(const JClass& cls, const char* sig = "()V", JArgs args = JArgs());
	JObject(const char* clsName, const char* sig = "()V", JArgs args = JArgs());
	virtual ~JObject();

	JObject& operator=(JObject& obj);
	operator jobject();

	bool instanceOf(const char* cls) const;
	bool isString() const;
	JString toStringShallow() const;
	QString toString() const;
	bool isArray() const;
	JArray toArray() const;

	bool isNull() const { return !m_object; }
	JClass getClass() const;

	QVariant call(const char* name, const char* sig, JArgs args = JArgs());
	QVariant getValue(const char* name, const char* sig);
	void setValue(const char* name, const char* sig, QVariant value);
protected:
	jobject m_object;
	jobject m_ref;
};
Q_DECLARE_METATYPE(JObject)

#endif // JOBJECT_H
