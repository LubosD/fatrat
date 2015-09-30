/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2011 Lubos Dolezel <lubos a dolezel.info>

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

#include "JMap.h"
#include "RuntimeException.h"
#include <QtDebug>

JMap::JMap()
	: JObject("java.util.HashMap", JSignature())
{

}

JMap::JMap(int initialCapacity)
	: JObject("java.util.HashMap", JSignature().addInt(), JArgs() << initialCapacity)
{

}

JMap::JMap(const JObject& that)
	: JObject(that)
{

}

void JMap::put(JObject key, JObject value)
{
	call("put", JSignature().add("java.lang.Object").add("java.lang.Object").ret("java.lang.Object"), key, value);
}

JObject JMap::nativeToBoxed(QVariant var)
{
	switch (var.type())
	{
	case QVariant::Bool:
		return nativeToBoxed(var.toBool());
	case QVariant::Double:
		return nativeToBoxed(var.toDouble());
	case QVariant::UInt:
	case QVariant::Int:
		return nativeToBoxed(var.toInt());
	case QVariant::ULongLong:
	case QVariant::LongLong:
		return nativeToBoxed(var.toLongLong());
	case QVariant::Map:
		break;
	case QVariant::String:
		return nativeToBoxed(var.toString());
	case QVariant::StringList:
		break;
	default:
		break;
	}

	qDebug() << "WARNING: JMap::nativeToBoxed(): the QVariant type was not converted to a java.lang.Object";
	return JObject();
}

template<typename T> void assignTo(JObject& val, QVariant& out)
{
	T tt;
	JMap::boxedToNative(val, tt);
	out = tt;
}

void JMap::boxedToNative(JObject& val, QVariant& out)
{
	if (val.instanceOf("java.lang.Integer"))
		assignTo<int>(val, out);
	else if (val.instanceOf("java.lang.Double"))
		assignTo<double>(val, out);
	else if (val.instanceOf("java.lang.Float"))
		assignTo<float>(val, out);
	else if (val.instanceOf("java.lang.Short"))
		assignTo<short>(val, out);
	else if (val.instanceOf("java.lang.Byte"))
		assignTo<jbyte>(val, out);
	else if (val.instanceOf("java.lang.Character"))
		assignTo<wchar_t>(val, out);
	else if (val.isString())
		out = val.toStringShallow().str();
	else if (val.isArray())
	{
		throw RuntimeException("Arrays are not supported");
	}
	else
		out = val;
}
