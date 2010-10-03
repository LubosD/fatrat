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

#ifndef JARRAY_H
#define JARRAY_H

#include "config.h"
#ifndef WITH_JPLUGINS
#	error This file is not supposed to be included!
#endif

#include <jni.h>
#include "JObject.h"

class JArray : public JObject
{
public:
	JArray(jarray arr);
	JArray(const char* type, size_t length);

	size_t length() const { return size(); }
	size_t size() const;

	static JArray createIntArray(size_t length) { return JArray("I", length); }
	static JArray createLongLongArray(size_t length) { return JArray("J", length); }
	static JArray createShortArray(size_t length) { return JArray("S", length); }
	static JArray createWchar_tArray(size_t length) { return JArray("C", length); }
	static JArray createByteArray(size_t length) { return JArray("B", length); }
	static JArray createFloatArray(size_t length) { return JArray("F", length); }
	static JArray createDoubleArray(size_t length) { return JArray("D", length); }
	static JArray createBoolArray(size_t length) { return JArray("B", length); }

	bool isInt() const { return instanceOf("[I"); }
	bool isLongLong() const { return instanceOf("[J"); }
	bool isShort() const { return instanceOf("[S"); }
	bool isWchar_t() const { return instanceOf("[C"); }
	bool isByte() const { return instanceOf("[B"); }
	bool isFloat() const { return instanceOf("[F"); }
	bool isDouble() const { return instanceOf("[D"); }
	bool isBool() const { return instanceOf("[Z"); }

	int getInt(int index) const;
	void setInt(int index, int value);
	qlonglong getLongLong(int index) const;
	void setLongLong(int index, qlonglong value);
	short getShort(int index) const;
	void setShort(int index, short value);
	wchar_t getWchar_t(int index) const;
	void setWchar_t(int index, wchar_t value);
	char getByte(int index) const;
	void setByte(int index, char value);
	float getFloat(int index) const;
	void setFloat(int index, float value);
	double getDouble(int index) const;
	void setDouble(int index, double value);
	bool getBool(int index) const;
	void setBool(int index, bool value);
	JObject getObject(int index) const;
	void setObject(int index, JObject value);
};

#endif // JARRAY_H
