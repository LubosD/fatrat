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

#include "JObject.h"
#include "JVM.h"
#include "RuntimeException.h"
#include "JString.h"
#include "JArray.h"
#include "JClass.h"
#include "JScope.h"
#include "JException.h"
#ifdef POSIX_LINUX
#	include <alloca.h>
#endif
#include <cstdlib>
#include <cassert>
#include <QtDebug>

JObject::JObject()
	: m_bWeak(false)
{
	m_object = 0;
}

JObject::JObject(const JObject& obj)
		: m_bWeak(obj.m_bWeak)
{
	JNIEnv* env = *JVM::instance();
	m_object = obj.m_object;
	if (m_object)
	{
		if (!m_bWeak)
			m_object = env->NewGlobalRef(m_object);
		else
			m_object = env->NewWeakGlobalRef(m_object);
	}
}

#ifdef WITH_CXX0X
JObject::JObject(JObject&& obj)
{
	m_object = obj.m_object;
	obj.m_object = 0;
	m_bWeak = obj.m_bWeak;
}
#endif

JObject::JObject(jobject obj, bool weak)
	: m_bWeak(weak)
{
	JNIEnv* env = *JVM::instance();
	m_object = obj;

	if (!weak)
		m_object = env->NewGlobalRef(m_object);
	else
		m_object = env->NewWeakGlobalRef(m_object);
}

void JObject::construct(const JClass& xcls, const char* sig, JArgs args)
{
	JNIEnv* env = *JVM::instance();
	jclass cls = xcls;
    
	m_bWeak = false;

	if (!cls)
		throw RuntimeException(QObject::tr("Invalid JClass passed"));

	jmethodID mid = env->GetMethodID(cls, "<init>", sig);
	if (!mid)
		throw RuntimeException(QObject::tr("Constructor %1 not found").arg(sig));

	jvalue jargs[args.size()];
	JValue vals[args.size()];

	for(int i=0;i<args.size();i++)
	{
		vals[i] = JClass::variantToValue(args[i]);
		jargs[i] = vals[i];
	}

	jobject obj;

	obj = env->NewObjectA(cls, mid, jargs);
	assert(!env->ExceptionOccurred());
	JObject ex = env->ExceptionOccurred();

	if (!ex.isNull())
	{
		env->ExceptionClear();
		ex.call("printStackTrace");
		QString message = ex.call("getMessage", JSignature().retString()).toString();
		QString className = ex.getClass().getClassName();

		throw JException(message, className, ex);
	}

	if (!obj)
		throw RuntimeException(QObject::tr("Failed to create an instance - constructor %1").arg(sig));
	m_object = env->NewGlobalRef(obj);
	env->DeleteLocalRef(obj);
}

JObject::JObject(const JClass& xcls, const char* sig, JArgs args)
	: m_object(0)
{
	construct(xcls, sig, args);
}

JObject::JObject(const char* clsName, const char* sig, JArgs args)
		: m_object(0)
{
	JClass cls(clsName);
	construct(cls, sig, args);
}

JObject::JObject(const JClass& xcls, JSignature sig, JArgs args)
	: m_object(0)
{
	QByteArray ba = sig.str().toLatin1();
	construct(xcls, ba.data(), args);
}

JObject::JObject(const char* clsName, JSignature sig, JArgs args)
		: m_object(0)
{
	JClass cls(clsName);
	QByteArray ba = sig.str().toLatin1();
	construct(cls, ba.data(), args);
}

JObject& JObject::operator=(const JObject& obj)
{
	JNIEnv* env = *JVM::instance();

	setNull();

    m_bWeak = obj.m_bWeak;
    if (m_bWeak)
        m_object = env->NewWeakGlobalRef(obj.m_object);
    else
        m_object = env->NewGlobalRef(obj.m_object);
	return *this;
}

#ifdef WITH_CXX0X
JObject& JObject::operator=(JObject&& obj)
{
	setNull();
	m_object = obj.m_object;
	obj.m_object = 0;
	m_bWeak = obj.m_bWeak;
	return *this;
}
#endif

JObject& JObject::operator=(jobject obj)
{
	JNIEnv* env = *JVM::instance();

	setNull();
	m_object = env->NewGlobalRef(obj);
	m_bWeak = false;

	return *this;
}


JObject::operator jobject()
{
	return m_object;
}

JObject::~JObject()
{
	setNull();
}

bool JObject::isString() const
{
	return instanceOf("java/lang/String");
}

JString JObject::toStringShallow() const
{
	if (!isString())
		return JString();
	return JString(jstring(m_object));
}

QString JObject::toString() const
{
	if (!m_object)
		return JString();
	return const_cast<JObject*>(this)->call("toString", JSignature().retString()).toString();
}

bool JObject::instanceOf(const char* cls) const
{
	JScope s;

	if (!m_object)
		return false;

	if (strchr(cls, '.'))
	{
		char* corr = static_cast<char*>(alloca(strlen(cls)+1));
		strcpy(corr, cls);
		while (char* p = strchr(corr, '.'))
			*p = '/';
		return instanceOf(corr);
	}
	else
	{
		JNIEnv* env = *JVM::instance();
		jclass clz = env->FindClass(cls);
		if (!clz)
			return false;

		return env->IsInstanceOf(m_object, clz);
	}
}

bool JObject::isArray() const
{
	/*JScope s;
	return instanceOf("[Z") ||
			instanceOf("[B") ||
			instanceOf("[C") ||
			instanceOf("[S") ||
			instanceOf("[I") ||
			instanceOf("[J") ||
			instanceOf("[F") ||
			instanceOf("[D");*/
	QString name = getClass().getClassName();
	return name.endsWith("[]");
}

JArray JObject::toArray() const
{
	JScope s;
	return JArray(jarray(m_object));
}

bool JObject::isSameObject(jobject that) const
{
	JNIEnv* env = *JVM::instance();
	return env->IsSameObject(m_object, that) == JNI_TRUE;
}

QVariant JObject::call(const char* name, JSignature sig, JArgs args)
{
	QByteArray ba = sig.str().toLatin1();
	return call(name, ba.data(), args);
}

QVariant JObject::call(const char* name, const char* sig, JArgs args)
{
	assert(m_object != 0);

	JScope s;
	JNIEnv* env = *JVM::instance();
	jclass cls = env->GetObjectClass(m_object);

	jmethodID mid = env->GetMethodID(cls, name, sig);
	if (!mid)
		throw RuntimeException(QObject::tr("Method %1 %2 not found").arg(name).arg(sig));

	jvalue jargs[args.size()];
	JValue vals[args.size()];

	for(int i=0;i<args.size();i++)
	{
		vals[i] = JClass::variantToValue(args[i]);
		jargs[i] = vals[i];
	}

	const char* rvtype = strchr(sig, ')');
	if (!rvtype)
		throw RuntimeException(QObject::tr("Invalid method return type").arg(name).arg(sig));
	rvtype++;

	JScope callScope;
	QVariant retval;
	switch (*rvtype)
	{
	case 'V':
		env->CallVoidMethodA(m_object, mid, jargs);
		break;
	case '[':
		{
			jobject obj = env->CallObjectMethodA(m_object, mid, jargs);
			retval = JArray(callScope.popWithRef(obj)).toVariant();
			break;
		}
	case 'L':
		{
			jobject obj = env->CallObjectMethodA(m_object, mid, jargs);
			JObject jobj(callScope.popWithRef(obj));

			if (jobj.isString() && !strcmp(rvtype+1, "java/lang/String;"))
				retval = jobj.toStringShallow().str();
			else
				retval = jobj.toVariant();
			break;
		}
	case 'Z':
		retval = env->CallBooleanMethodA(m_object, mid, jargs) == JNI_TRUE;
		break;
	case 'B':
		retval = env->CallByteMethodA(m_object, mid, jargs);
		break;
	case 'C':
		retval = QChar(env->CallCharMethodA(m_object, mid, jargs));
		break;
	case 'S':
		retval = env->CallShortMethodA(m_object, mid, jargs);
		break;
	case 'I':
		retval = env->CallIntMethodA(m_object, mid, jargs);
		break;
	case 'J':
		retval = (qlonglong) env->CallLongMethodA(m_object, mid, jargs);
		break;
	case 'F':
		retval = env->CallFloatMethodA(m_object, mid, jargs);
		break;
	case 'D':
		retval = env->CallDoubleMethodA(m_object, mid, jargs);
		break;
	default:
		throw RuntimeException(QObject::tr("Unknown Java data type: %1").arg(rvtype));
	}

	JObject ex = env->ExceptionOccurred();

	if (!ex.isNull())
	{
		env->ExceptionClear();
		ex.call("printStackTrace");
		QString message = ex.call("getMessage", JSignature().retString()).toString();
		QString className = ex.getClass().getClassName();

		throw JException(message, className, ex);
	}

	return retval;
}

QVariant JObject::getValue(const char* name, JSignature sig) const
{
	QByteArray ba = sig.str().toLatin1();
	return getValue(name, ba.data());
}

QVariant JObject::getValue(const char* name, const char* sig) const
{
	JScope s;
	JNIEnv* env = *JVM::instance();
	jclass cls = env->GetObjectClass(m_object);

	jfieldID fid = env->GetFieldID(cls, name, sig);
	if (!fid)
		throw RuntimeException(QObject::tr("Field %1 %2 not found").arg(name).arg(sig));

	switch (sig[0])
	{
	case '[':
		{
			QVariant var;
			jobject obj = env->GetObjectField(m_object, fid);
			var.setValue<JArray>(JArray(obj));
			return var;
		}
	case 'L':
		{
			jclass string_class = env->FindClass("java/lang/String");
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
		return env->GetBooleanField(m_object, fid) == JNI_TRUE;
	case 'B':
		return env->GetByteField(m_object, fid);
	case 'C':
		return QChar(env->GetCharField(m_object, fid));
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

void JObject::setValue(const char* name, JSignature sig, QVariant value)
{
	QByteArray ba = sig.str().toLatin1();
	setValue(name, ba.data(), value);
}

void JObject::setValue(const char* name, const char* sig, QVariant value)
{
	JScope s;
	JNIEnv* env = *JVM::instance();
	jclass cls = env->GetObjectClass(m_object);

	jfieldID fid = env->GetFieldID(cls, name, sig);
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

JClass JObject::getClass() const
{
	assert(m_object != 0);

	JScope s;
	JNIEnv* env = *JVM::instance();
	return JClass( env->GetObjectClass(m_object) );
}

QVariant JObject::toVariant() const
{
	return QVariant::fromValue<JObject>(*this);
}

void JObject::setNull()
{
	if (m_object)
	{
		JNIEnv* env = *JVM::instance();
		jobject o = m_object;

		m_object = 0;

		if (m_bWeak)
			env->DeleteWeakGlobalRef(jweak(o));
		else
			env->DeleteGlobalRef(o);
	}
}

bool JObject::isNull() const
{
	JNIEnv* env = *JVM::instance();
	return !m_object || (m_bWeak && env->IsSameObject(m_object, 0) == JNI_TRUE);
}

jobject JObject::getLocalRef()
{
	if (!m_object)
		return 0;
	else
	{
		JNIEnv* env = *JVM::instance();
		return env->NewLocalRef(m_object);
	}
}
