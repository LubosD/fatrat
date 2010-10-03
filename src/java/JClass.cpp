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

#include "JClass.h"
#include "JString.h"
#include "JObject.h"
#include "JVM.h"
#include "RuntimeException.h"
#include <alloca.h>

JClass::JClass(const JClass& cls)
		: m_ref(0)
{
	JClass(cls.m_class);
}

JClass::JClass(QString clsName)
	: m_ref(0)
{
	JNIEnv* env = *JVM::instance();
	QByteArray name = clsName.toUtf8();

	m_class = env->FindClass(name.constData());
	if (!m_class)
		throw RuntimeException(QObject::tr("Java class %1 not found or failed to load").arg(clsName));
	m_ref = env->NewGlobalRef(m_class);
}

JClass::JClass(jclass cls)
	: m_ref(0)
{
	JNIEnv* env = *JVM::instance();
	m_class = cls;
	m_ref = env->NewGlobalRef(m_class);
}

JClass::~JClass()
{
	if (m_ref)
	{
		JNIEnv* env = *JVM::instance();
		env->DeleteGlobalRef(m_ref);
	}
}

JClass::operator jclass() const
{
	return m_class;
}

QVariant JClass::callStatic(const char* name, const char* sig, QList<QVariant>& args)
{
	JNIEnv* env = *JVM::instance();
	jmethodID mid = env->GetStaticMethodID(m_class, name, sig);
	if (!mid)
		throw RuntimeException(QObject::tr("Method %1 %2 not found").arg(name).arg(sig));

	jvalue* jargs = (jvalue*) alloca(args.size()*sizeof(jvalue));

	for(int i=0;i<args.size();i++)
		jargs[i] = variantToValue(args[i]);

	const char* rvtype = strchr(sig, ')');
	if (!rvtype)
		throw RuntimeException(QObject::tr("Invalid method return type").arg(name).arg(sig));
	rvtype++;

	switch (*rvtype)
	{
	case 'V':
		env->CallStaticVoidMethodA(m_class, mid, jargs);
		return QVariant();
	case 'L':
		{
			jclass string_class = env->FindClass("Ljava/lang/String");
			jobject obj = env->CallStaticObjectMethodA(m_class, mid, jargs);
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
		return (bool) env->CallStaticBooleanMethodA(m_class, mid, jargs);
	case 'B':
		return env->CallStaticByteMethodA(m_class, mid, jargs);
	case 'C':
		return env->CallStaticCharMethodA(m_class, mid, jargs);
	case 'S':
		return env->CallStaticShortMethodA(m_class, mid, jargs);
	case 'I':
		return env->CallStaticIntMethodA(m_class, mid, jargs);
	case 'J':
		return (qlonglong) env->CallStaticLongMethodA(m_class, mid, jargs);
	case 'F':
		return env->CallStaticFloatMethodA(m_class, mid, jargs);
	case 'D':
		return env->CallStaticDoubleMethodA(m_class, mid, jargs);
	default:
		throw RuntimeException(QObject::tr("Unknown Java data type: %1").arg(sig[0]));
	}
}

jvalue JClass::variantToValue(QVariant& v)
{
	jvalue jv;
	switch (v.type())
	{
	case QVariant::Int:
	case QVariant::UInt:
		jv.i = v.toInt();
		break;
	case QVariant::Double:
		jv.d = v.toDouble();
		break;
	case QVariant::LongLong:
		jv.j = v.toLongLong();
		break;
	case QVariant::Bool:
		jv.z = v.toBool();
		break;
	case QVariant::String:
		jv.l = (jobject) (jstring) JString(v.toString());
		break;
	default:
		if (v.canConvert<JObject>())
			jv.l = (jobject) v.value<JObject>();
		else
			throw RuntimeException(QObject::tr("Unknown data type"));
	}
	return jv;
}

QVariant JClass::getStaticValue(const char* name, const char* sig)
{
	JNIEnv* env = *JVM::instance();
	jfieldID fid = env->GetStaticFieldID(m_class, name, sig);
	if (!fid)
		throw RuntimeException(QObject::tr("Field %1 %2 not found").arg(name).arg(sig));

	switch (sig[0])
	{
	case '[':
		break;
	case 'L':
		{
			jclass string_class = env->FindClass("Ljava/lang/String");
			jobject obj = env->GetStaticObjectField(m_class, fid);
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
		return (bool) env->GetStaticBooleanField(m_class, fid);
	case 'B':
		return env->GetStaticByteField(m_class, fid);
	case 'C':
		return env->GetStaticCharField(m_class, fid);
	case 'S':
		return env->GetStaticShortField(m_class, fid);
	case 'I':
		return env->GetStaticIntField(m_class, fid);
	case 'J':
		return (qlonglong) env->GetStaticLongField(m_class, fid);
	case 'F':
		return env->GetStaticFloatField(m_class, fid);
	case 'D':
		return env->GetStaticDoubleField(m_class, fid);
	default:
		throw RuntimeException(QObject::tr("Unknown Java data type: %1").arg(sig[0]));
	}
}

void JClass::setStaticValue(const char* name, const char* sig, QVariant value)
{
	JNIEnv* env = *JVM::instance();
	jfieldID fid = env->GetStaticFieldID(m_class, name, sig);
	if (!fid)
		throw RuntimeException(QObject::tr("Field %1 %2 not found").arg(name).arg(sig));

	switch (sig[0])
	{
	case 'L':
		{
			if (value.type() == QVariant::String)
			{
				JString js(value.toString());
				env->SetStaticObjectField(m_class, fid, js);
			}
			else
			{
				jobject o = (jobject) value.value<JObject>();
				env->SetStaticObjectField(m_class, fid, o);
			}
			break;
		}
	case 'Z':
		env->SetStaticBooleanField(m_class, fid, value.toBool());
		break;
	case 'B':
		env->SetStaticByteField(m_class, fid, value.toInt());
		break;
	case 'C':
		env->SetStaticCharField(m_class, fid, value.toInt());
		break;
	case 'S':
		env->SetStaticShortField(m_class, fid, value.toInt());
		break;
	case 'I':
		env->SetStaticIntField(m_class, fid, value.toInt());
		break;
	case 'J':
		env->SetStaticLongField(m_class, fid, value.toLongLong());
		break;
	case 'F':
		env->SetStaticFloatField(m_class, fid, value.toFloat());
		break;
	case 'D':
		env->SetStaticDoubleField(m_class, fid, value.toDouble());
		break;
	default:
		throw RuntimeException(QObject::tr("Unknown Java data type: %1").arg(sig[0]));
	}
}
