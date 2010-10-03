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

#include "JObject.h"
#include "JVM.h"
#include "RuntimeException.h"
#include "JString.h"
#include "JClass.h"
#include <alloca.h>

JObject::JObject()
	: m_object(0), m_ref(0)
{

}

JObject::JObject(const JObject& obj)
{
	JNIEnv* env = *JVM::instance();
	m_object = obj.m_object;
	if (m_object)
		m_ref = env->NewGlobalRef(m_object);
}

JObject::JObject(jobject obj)
{
	JNIEnv* env = *JVM::instance();
	m_object = obj;
	m_ref = env->NewGlobalRef(m_object);
}

JObject::JObject(const JClass& xcls, const char* sig, QList<QVariant>& args)
	: m_object(0), m_ref(0)
{
	JNIEnv* env = *JVM::instance();
	jclass cls = xcls;

	if (!cls)
		throw RuntimeException(QObject::tr("Invalid JClass passed"));

	jmethodID mid = env->GetMethodID(cls, "<init>", sig);
	if (!mid)
		throw RuntimeException(QObject::tr("Constructor %1 not found").arg(sig));

	jvalue* jargs = (jvalue*) alloca(args.size()*sizeof(jvalue));

	for(int i=0;i<args.size();i++)
		jargs[i] = JClass::variantToValue(args[i]);

	m_object = env->NewObjectA(cls, mid, jargs);
	if (!m_object)
		throw RuntimeException(QObject::tr("Failed to create an instance").arg(sig));
	m_ref = env->NewGlobalRef(m_object);
}

JObject::JObject(const char* clsName, const char* sig, QList<QVariant>& args)
		: m_object(0), m_ref(0)
{
	JNIEnv* env = *JVM::instance();
	jclass cls = env->FindClass(clsName);

	if (!cls)
		throw RuntimeException(QObject::tr("Class %1 not found").arg(clsName));

	jmethodID mid = env->GetMethodID(cls, "<init>", sig);
	if (!mid)
		throw RuntimeException(QObject::tr("Constructor %1 not found").arg(sig));

	jvalue* jargs = (jvalue*) alloca(args.size()*sizeof(jvalue));

	for(int i=0;i<args.size();i++)
		jargs[i] = JClass::variantToValue(args[i]);

	m_object = env->NewObjectA(cls, mid, jargs);
	if (!m_object)
		throw RuntimeException(QObject::tr("Failed to create an instance").arg(sig));
	m_ref = env->NewGlobalRef(m_object);
}

JObject::operator jobject()
{
	return m_object;
}

JObject::~JObject()
{
	if (m_ref)
	{
		JNIEnv* env = *JVM::instance();
		env->DeleteGlobalRef(m_ref);
	}
}

bool JObject::isString() const
{
	if (!m_object)
		return false;

	JNIEnv* env = *JVM::instance();
	jclass string_class = env->FindClass("Ljava/lang/String");

	return env->IsInstanceOf(m_object, string_class);
}

bool JObject::isArray() const
{

}

QVariant JObject::call(const char* name, const char* sig, QList<QVariant>& args)
{
	JNIEnv* env = *JVM::instance();
	jclass cls = env->GetObjectClass(m_object);

	jmethodID mid = env->GetMethodID(cls, name, sig);
	if (!mid)
		throw RuntimeException(QObject::tr("Method %1 %2 not found").arg(name).arg(sig));

	jvalue* jargs = (jvalue*) alloca(args.size()*sizeof(jvalue));

	for(int i=0;i<args.size();i++)
		jargs[i] = JClass::variantToValue(args[i]);

	const char* rvtype = strchr(sig, ')');
	if (!rvtype)
		throw RuntimeException(QObject::tr("Invalid method return type").arg(name).arg(sig));
	rvtype++;

	switch (*rvtype)
	{
	case 'V':
		env->CallVoidMethodA(m_object, mid, jargs);
		return QVariant();
	case 'L':
		{
			jclass string_class = env->FindClass("Ljava/lang/String");
			jobject obj = env->CallObjectMethodA(m_object, mid, jargs);
			if (env->IsInstanceOf(obj, string_class))
				return JString(jstring(obj)).str();
			else
			{
				QVariant var;
				var.setValue<JObject>(JObject(obj));
				return var;
			}
		}
	case 'Z':
		return (bool) env->CallBooleanMethodA(m_object, mid, jargs);
	case 'B':
		return env->CallByteMethodA(m_object, mid, jargs);
	case 'C':
		return env->CallCharMethodA(m_object, mid, jargs);
	case 'S':
		return env->CallShortMethodA(m_object, mid, jargs);
	case 'I':
		return env->CallIntMethodA(m_object, mid, jargs);
	case 'J':
		return (qlonglong) env->CallLongMethodA(m_object, mid, jargs);
	case 'F':
		return env->CallFloatMethodA(m_object, mid, jargs);
	case 'D':
		return env->CallDoubleMethodA(m_object, mid, jargs);
	default:
		throw RuntimeException(QObject::tr("Unknown Java data type: %1").arg(sig[0]));
	}
}

QVariant JObject::getValue(const char* name, const char* sig)
{
	JNIEnv* env = *JVM::instance();
	jclass cls = env->GetObjectClass(m_object);

	jfieldID fid = env->GetFieldID(cls, name, sig);
	if (!fid)
		throw RuntimeException(QObject::tr("Field %1 %2 not found").arg(name).arg(sig));

	switch (sig[0])
	{
	case '[':
		break;
	case 'L':
		{
			jclass string_class = env->FindClass("Ljava/lang/String");
			jobject obj = env->GetObjectField(m_object, fid);
			if (env->IsInstanceOf(obj, string_class))
				return JString(jstring(obj)).str();
			else
			{
				QVariant var;
				var.setValue<JObject>(JObject(obj));
				return var;
			}
		}
	case 'Z':
		return (bool) env->GetBooleanField(m_object, fid);
	case 'B':
		return env->GetByteField(m_object, fid);
	case 'C':
		return env->GetCharField(m_object, fid);
	case 'S':
		return env->GetShortField(m_object, fid);
	case 'I':
		return env->GetIntField(m_object, fid);
	case 'J':
		return (qlonglong) env->GetLongField(m_object, fid);
	case 'F':
		return env->GetFloatField(m_object, fid);
	case 'D':
		return env->GetDoubleField(m_object, fid);
	default:
		throw RuntimeException(QObject::tr("Unknown Java data type: %1").arg(sig[0]));
	}
}

void JObject::setValue(const char* name, const char* sig, QVariant value)
{
	JNIEnv* env = *JVM::instance();
	jclass cls = env->GetObjectClass(m_object);

	jfieldID fid = env->GetFieldID(cls, name, sig);
	if (!fid)
		throw RuntimeException(QObject::tr("Field %1 %2 not found").arg(name).arg(sig));

	switch (sig[0])
	{
	case 'L':
		{
			if (value.type() == QVariant::String)
			{
				JString js(value.toString());
				env->SetObjectField(m_object, fid, js);
			}
			else
			{
				jobject o = (jobject) value.value<JObject>();
				env->SetObjectField(m_object, fid, o);
			}
			break;
		}
	case 'Z':
		env->SetBooleanField(m_object, fid, value.toBool());
		break;
	case 'B':
		env->SetByteField(m_object, fid, value.toInt());
		break;
	case 'C':
		env->SetCharField(m_object, fid, value.toInt());
		break;
	case 'S':
		env->SetShortField(m_object, fid, value.toInt());
		break;
	case 'I':
		env->SetIntField(m_object, fid, value.toInt());
		break;
	case 'J':
		env->SetLongField(m_object, fid, value.toLongLong());
		break;
	case 'F':
		env->SetFloatField(m_object, fid, value.toFloat());
		break;
	case 'D':
		env->SetDoubleField(m_object, fid, value.toDouble());
		break;
	default:
		throw RuntimeException(QObject::tr("Unknown Java data type: %1").arg(sig[0]));
	}
}
