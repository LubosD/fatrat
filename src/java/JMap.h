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

#ifndef JMAP_H
#define JMAP_H

#include "config.h"
#ifndef WITH_JPLUGINS
#	error This file is not supposed to be included!
#endif
#include <QMap>
#include "JObject.h"
#include "JSignature.h"
#include "JString.h"
#include <cassert>
#include <QtDebug>

class JMap : public JObject
{
public:
	// Constructs a java.util.HashMap
	JMap();
	JMap(const JObject& that);
	JMap(int initialCapacity);

	// TODO: add all Java methods
	void put(JObject key, JObject value);
	template<typename K, typename V> void put(K key, V value)
	{
		put(nativeToBoxed(key), nativeToBoxed(value));
	}

	template<typename Key, typename T> static JMap fromQMap(const QMap<Key,T>& map)
	{
		JMap rv (map.size());
		typedef typename QMap<Key,T>::ConstIterator myiter;
		for (myiter it = map.constBegin(); it != map.constEnd(); it++)
			rv.put(it.key(), it.value());
		return rv;
	}
	template<typename Key, typename T> void toQMap(QMap<Key,T>& map)
	{
		JObject set = this->call("entrySet", JSignature().ret("java.util.Set")).value<JObject>();
		JObject iter = set.call("iterator", JSignature().ret("java.util.Iterator")).value<JObject>();

		map.clear();

		while (iter.call("hasNext", JSignature().retBoolean()).toBool())
		{
			JObject entry = iter.call("next", JSignature().ret("java.lang.Object")).value<JObject>();
			JObject key = entry.call("getKey", JSignature().ret("java.lang.Object")).value<JObject>();
			JObject value = entry.call("getValue", JSignature().ret("java.lang.Object")).value<JObject>();

			assert(!key.isNull());

			Key k;
			T v;

			boxedToNative(key, k);
			boxedToNative(value, v);
			map[k] = v;
		}
	}
	static JObject nativeToBoxed(int i)
	{
		return JObject("java.lang.Integer", JSignature().addInt(), JArgs() << i);
	}
	static JObject nativeToBoxed(long long l)
	{
		return JObject("java.lang.Long", JSignature().addLong(), JArgs() << l);
	}
	static JObject nativeToBoxed(double d)
	{
		return JObject("java.lang.Double", JSignature().addDouble(), JArgs() << d);
	}
	static JObject nativeToBoxed(float f)
	{
		return JObject("java.lang.Float", JSignature().addFloat(), JArgs() << f);
	}
	static JObject nativeToBoxed(short s)
	{
		return JObject("java.lang.Short", JSignature().addShort(), JArgs() << s);
	}
	static JObject nativeToBoxed(jbyte c)
	{
		return JObject("java.lang.Byte", JSignature().addByte(), JArgs() << c);
	}
	static JObject nativeToBoxed(wchar_t c)
	{
		return JObject("java.lang.Char", JSignature().addChar(), JArgs() << c);
	}
	static JObject nativeToBoxed(bool b)
	{
		return JObject("java.lang.Boolean", JSignature().addBoolean(), JArgs() << b);
	}
	static JObject nativeToBoxed(QVariant var);
	static JObject nativeToBoxed(QString s)
	{
		return JString(s);
	}
	static JObject nativeToBoxed(QByteArray s)
	{
		return JString(QString(s));
	}

	static void boxedToNative(JObject& val, QVariant& out);
	static void boxedToNative(JObject& val, QString& out)
	{
		out = val.toStringShallow().str();
	}
	static void boxedToNative(JObject& val, int& out)
	{
		out = val.call("intValue", JSignature().retInt()).toInt();
	}
	static void boxedToNative(JObject& val, short& out)
	{
		out = val.call("shortValue", JSignature().retShort()).toInt();
	}
	static void boxedToNative(JObject& val, long long& out)
	{
		out = val.call("longValue", JSignature().retLong()).toLongLong();
	}
	static void boxedToNative(JObject& val, double& out)
	{
		out = val.call("doubleValue", JSignature().retDouble()).toDouble();
	}
	static void boxedToNative(JObject& val, float& out)
	{
		out = val.call("floatValue", JSignature().retFloat()).toFloat();
	}
	static void boxedToNative(JObject& val, wchar_t& out)
	{
		out = val.call("charValue", JSignature().retChar()).toInt();
	}
	static void boxedToNative(JObject& val, jbyte& out)
	{
		out = val.call("byteValue", JSignature().retByte()).toInt();
	}
	static void boxedToNative(JObject& val, bool& out)
	{
		out = val.call("booleanValue", JSignature().retBoolean()).toBool();
	}
};

#endif // JMAP_H
