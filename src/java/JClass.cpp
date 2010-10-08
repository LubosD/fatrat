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
#include "JArray.h"
#include "JScope.h"
#include "JException.h"
#include "RuntimeException.h"
#include <alloca.h>
#include <QtDebug>

JClass::JClass(const JClass& cls)
{
	JNIEnv* env = *JVM::instance();
	m_class = (jclass) env->NewGlobalRef(cls.m_class);
}

JClass::JClass(QString clsName)
{
	JNIEnv* env = *JVM::instance();

	clsName.replace('.', '/');
	QByteArray name = clsName.toUtf8();

	jclass obj = env->FindClass(name.constData());
	if (!obj)
		throw RuntimeException(QObject::tr("Java class %1 not found or failed to load").arg(clsName));
	m_class = (jclass) env->NewGlobalRef(obj);
	env->DeleteLocalRef(obj);
}

JClass::JClass(jclass cls)
{
	JNIEnv* env = *JVM::instance();
	m_class = (jclass) env->NewGlobalRef(cls);
}

JClass::JClass(jobject cls)
{
	JNIEnv* env = *JVM::instance();
	m_class = (jclass) env->NewGlobalRef(cls);
}

JClass::~JClass()
{
	if (m_class)
	{
		JNIEnv* env = *JVM::instance();
		env->DeleteGlobalRef(m_class);
	}
}

JClass::operator jclass() const
{
	return m_class;
}

QVariant JClass::callStatic(const char* name, JSignature sig, JArgs args)
{
	QByteArray ba = sig.str().toLatin1();
	return callStatic(name, ba.data(), args);
}


QVariant JClass::callStatic(const char* name, const char* sig, QList<QVariant> args)
{
	JScope s;

	JNIEnv* env = *JVM::instance();
	jmethodID mid = env->GetStaticMethodID(m_class, name, sig);
	if (!mid)
		throw RuntimeException(QObject::tr("Method %1 %2 not found").arg(name).arg(sig));

	JValue vals[args.size()];
	jvalue jargs[args.size()];

	for(int i=0;i<args.size();i++)
	{
		vals[i] = variantToValue(args[i]);
		jargs[i] = vals[i];
	}

	const char* rvtype = strchr(sig, ')');
	if (!rvtype)
		throw RuntimeException(QObject::tr("Invalid method return type").arg(name).arg(sig));
	rvtype++;

	QVariant retval;

	switch (*rvtype)
	{
	case 'V':
		env->CallStaticVoidMethodA(m_class, mid, jargs);
		break;
	case '[':
		{
			jobject obj = env->CallStaticObjectMethodA(m_class, mid, jargs);
			retval.setValue<JArray>(JArray(obj));
			break;
		}
	case 'L':
		{
			jclass string_class = env->FindClass("java/lang/String");
			jobject obj = env->CallStaticObjectMethodA(m_class, mid, jargs);
			if (env->IsInstanceOf(obj, string_class))
				retval = JString(jstring(obj)).str();
			else
			{
				QVariant var;
				var.setValue<JObject>(JObject(obj));
				retval = var;
			}
			break;
		}
	case 'Z':
		retval = (bool) env->CallStaticBooleanMethodA(m_class, mid, jargs);
		break;
	case 'B':
		retval = env->CallStaticByteMethodA(m_class, mid, jargs);
		break;
	case 'C':
		retval = env->CallStaticCharMethodA(m_class, mid, jargs);
		break;
	case 'S':
		retval = env->CallStaticShortMethodA(m_class, mid, jargs);
		break;
	case 'I':
		retval = env->CallStaticIntMethodA(m_class, mid, jargs);
		break;
	case 'J':
		retval = (qlonglong) env->CallStaticLongMethodA(m_class, mid, jargs);
		break;
	case 'F':
		retval = env->CallStaticFloatMethodA(m_class, mid, jargs);
		break;
	case 'D':
		retval = env->CallStaticDoubleMethodA(m_class, mid, jargs);
		break;
	default:
		throw RuntimeException(QObject::tr("Unknown Java data type: %1").arg(*rvtype));
	}

	JObject ex = env->ExceptionOccurred();

	if (!ex.isNull())
	{
		ex.call("printStackTrace", "()V", QList<QVariant>());
		QString message = ex.call("getMessage", "()Ljava/lang/String;", QList<QVariant>()).toString();
		QString className = ex.getClass().getClassName();
		env->ExceptionClear();

		throw JException(message, className);
	}

	return retval;
}

JValue JClass::variantToValue(QVariant& v)
{
	//JScope s;

	switch (v.type())
	{
	case QVariant::Int:
	case QVariant::UInt:
		return JValue(v.toInt());
	case QVariant::Double:
		return JValue(v.toDouble());
	case QVariant::LongLong:
		return JValue(v.toLongLong());
	case QVariant::Bool:
		return JValue(v.toBool());
	case QVariant::String:
		{
			JString str = JString(v.toString());
			return JValue(str);
		}
	default:
		if (v.canConvert<JObject>())
		{
			JObject obj = v.value<JObject>();
			return JValue(obj);
		}
		else if (v.canConvert<JArray>())
		{
			JArray arr = v.value<JArray>();
			return JValue(arr);
		}
		else
			throw RuntimeException(QObject::tr("Unknown data type"));
	}
}

JObject JClass::toClassObject() const
{
	return JObject(m_class);
}

QVariant JClass::getStaticValue(const char* name, JSignature sig) const
{
	QByteArray ba = sig.str().toLatin1();
	return getStaticValue(name, ba.data());
}

QVariant JClass::getStaticValue(const char* name, const char* sig) const
{
	JScope s;

	JNIEnv* env = *JVM::instance();
	jfieldID fid = env->GetStaticFieldID(m_class, name, sig);
	if (!fid)
		throw RuntimeException(QObject::tr("Field %1 %2 not found").arg(name).arg(sig));

	switch (sig[0])
	{
	case '[':
		{
			QVariant var;
			jobject obj = env->GetStaticObjectField(m_class, fid);
			var.setValue<JArray>(JArray(obj));
			return var;
		}
	case 'L':
		{
			jclass string_class = env->FindClass("java/lang/String");
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

void JClass::setStaticValue(const char* name, JSignature sig, QVariant value)
{
	QByteArray ba = sig.str().toLatin1();
	setStaticValue(name, ba.data(), value);
}

void JClass::setStaticValue(const char* name, const char* sig, QVariant value)
{
	JScope s;

	JNIEnv* env = *JVM::instance();
	jfieldID fid = env->GetStaticFieldID(m_class, name, sig);
	if (!fid)
		throw RuntimeException(QObject::tr("Field %1 %2 not found").arg(name).arg(sig));

	switch (sig[0])
	{
	case '[':
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

QString JClass::getClassName() const
{
	JScope s;
	JObject obj(m_class);
	QList<QVariant> args;
	return obj.call("getCanonicalName", JSignature().retString(), args).toString();
}

JObject JClass::getAnnotation(JClass cls)
{
	return toClassObject().call("getAnnotation",
				    JSignature().add("java.lang.Class").ret("java.lang.annotation.Annotation"),
				    JArgs() << cls.toVariant()).value<JObject>();
}

JObject JClass::getAnnotation(QString className)
{
	JClass ann(className);
	return getAnnotation(ann);
}

QVariant JClass::toVariant() const
{
	return QVariant::fromValue<JObject>(toClassObject());
}
