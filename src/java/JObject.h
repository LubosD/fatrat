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
#include "JSignature.h"

class JString;
class JArray;

class JObject
{
public:
	JObject();
	JObject(jobject obj);
	JObject(const JObject& obj);
	JObject(const JClass& cls, const char* sig, JArgs args = JArgs());
	JObject(const JClass& cls, JSignature sig, JArgs args = JArgs());
	JObject(const char* clsName, const char* sig, JArgs args = JArgs());
	JObject(const char* clsName, JSignature sig, JArgs args = JArgs());
	virtual ~JObject();

	JObject& operator=(const JObject& obj);
	JObject& operator=(jobject obj);
	operator jobject();
	operator QVariant() { return toVariant(); }

	void setNull();

	bool instanceOf(const char* cls) const;
	bool isSameObject(jobject that) const;
	bool isString() const;
	JString toStringShallow() const;
	QString toString() const;
	bool isArray() const;
	JArray toArray() const;

	bool isNull() const { return !m_object; }
	JClass getClass() const;

	template<typename T1> QVariant call(const char *name, const JSignature& sig, T1 a1)
	{
		return call(name, sig, JArgs() << a1);
	}
	template<typename T1, typename T2> QVariant call(const char *name, const JSignature& sig, T1 a1, T2 a2)
	{
		return call(name, sig, JArgs() << a1 << a2);
	}
	template<typename T1, typename T2, typename T3> QVariant call(const char *name, const JSignature& sig, T1 a1, T2 a2, T3 a3)
	{
		return call(name, sig, JArgs() << a1 << a2 << a3);
	}
	template<typename T1, typename T2, typename T3, typename T4> QVariant call(const char *name, const JSignature& sig, T1 a1, T2 a2, T3 a3, T4 a4)
	{
		return call(name, sig, JArgs() << a1 << a2 << a3 << a4);
	}
	template<typename T1, typename T2, typename T3, typename T4, typename T5> QVariant call(const char *name, const JSignature& sig, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5)
	{
		return call(name, sig, JArgs() << a1 << a2 << a3 << a4 << a5);
	}
	template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6> QVariant call(const char *name, const JSignature& sig, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6)
	{
		return call(name, sig, JArgs() << a1 << a2 << a3 << a4 << a5 << a6);
	}

	/*
	 The following C++0x code will replace the broken mess above:

	 template<typename T> void addArg(JArgs& args, T arg)
	 {
		args << arg;
	 }
	 template<typename T, typename... Args> void addArg(JArgs& args, T arg, Args... xargs)
	 {
		addArg(args, arg);
		addArg(args, xargs);
	 }
	 template<typename... Args> QVariant call(const char *name, const JSignature& sig, Args... xargs)
	 {
		JArgs ja;
		addArg(ja, xargs);
		return call(name, sig, ja);
	 }

	 */

	QVariant call(const char* name, JSignature sig, JArgs args = JArgs());
	QVariant call(const char* name, const char* sig = "()V", JArgs args = JArgs());
	QVariant getValue(const char* name, JSignature sig) const;
	QVariant getValue(const char* name, const char* sig) const;
	void setValue(const char* name, JSignature sig, QVariant value);
	void setValue(const char* name, const char* sig, QVariant value);

	QVariant toVariant() const;
private:
	void construct(const JClass& cls, const char* sig, JArgs args);
protected:
	jobject m_object;
};
Q_DECLARE_METATYPE(JObject)

#endif // JOBJECT_H
