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

#ifndef JSIGNATURE_H
#define JSIGNATURE_H

#include "config.h"
#ifndef WITH_JPLUGINS
#	error This file is not supposed to be included!
#endif

#include <QString>

class JSignature
{
public:
	JSignature();
	JSignature& add(QString cls);
	inline JSignature& addString() { return add("java/lang/String"); }
	inline JSignature& addBoolean() { return addPrimitive("Z"); }
	inline JSignature& addByte() { return addPrimitive("B"); }
	inline JSignature& addChar() { return addPrimitive("C"); }
	inline JSignature& addShort() { return addPrimitive("S"); }
	inline JSignature& addInt() { return addPrimitive("I"); }
	inline JSignature& addLong() { return addPrimitive("J"); }
	inline JSignature& addFloat() { return addPrimitive("F"); }
	inline JSignature& addDouble() { return addPrimitive("D"); }

	// arrays
	inline JSignature& addA(QString cls) { return add("["+cls); }
	inline JSignature& addStringA() { return add("[java/lang/String"); }
	inline JSignature& addBooleanA() { return addPrimitive("[Z"); }
	inline JSignature& addByteA() { return addPrimitive("[B"); }
	inline JSignature& addCharA() { return addPrimitive("[C"); }
	inline JSignature& addShortA() { return addPrimitive("[S)"); }
	inline JSignature& addIntA() { return addPrimitive("[I"); }
	inline JSignature& addLongA() { return addPrimitive("[J"); }
	inline JSignature& addFloatA() { return addPrimitive("[F"); }
	inline JSignature& addDoubleA() { return addPrimitive("[D"); }

	// return values
	JSignature& ret(QString cls);
	inline JSignature& retString() { return ret("java/lang/String"); }
	inline JSignature& retBoolean() { return retPrimitive("Z"); }
	inline JSignature& retByte() { return retPrimitive("B"); }
	inline JSignature& retChar() { return retPrimitive("C"); }
	inline JSignature& retShort() { return retPrimitive("S"); }
	inline JSignature& retInt() { return retPrimitive("I"); }
	inline JSignature& retLong() { return retPrimitive("J"); }
	inline JSignature& retFloat() { return retPrimitive("F"); }
	inline JSignature& retDouble() { return retPrimitive("D"); }

	// return arrays
	inline JSignature& retA(QString cls) { return ret("["+cls); }
	inline JSignature& retStringA() { return ret("[java/lang/String"); }
	inline JSignature& retBooleanA() { return retPrimitive("[Z"); }
	inline JSignature& retByteA() { return retPrimitive("[B"); }
	inline JSignature& retCharA() { return retPrimitive("[C"); }
	inline JSignature& retShortA() { return retPrimitive("[S"); }
	inline JSignature& retIntA() { return retPrimitive("[I"); }
	inline JSignature& retLongA() { return retPrimitive("[J"); }
	inline JSignature& retFloatA() { return retPrimitive("[F"); }
	inline JSignature& retDoubleA() { return retPrimitive("[D"); }

	// variables
	static JSignature sig(QString cls);
	static const char* sigBoolean() { return "Z"; }
	static const char* sigByte() { return "B"; }
	static const char* sigChar() { return "C"; }
	static const char* sigShort() { return "S"; }
	static const char* sigInt() { return "I"; }
	static const char* sigLong() { return "J"; }
	static const char* sigFloat() { return "F"; }
	static const char* sigDouble() { return "D"; }

	// array variables
	static JSignature sigA(QString cls) { return sig('['+cls); }
	static const char* sigBooleanA() { return "[Z"; }
	static const char* sigByteA() { return "[B"; }
	static const char* sigCharA() { return "[C"; }
	static const char* sigShortA() { return "[S"; }
	static const char* sigIntA() { return "[I"; }
	static const char* sigLongA() { return "[J"; }
	static const char* sigFloatA() { return "[F"; }
	static const char* sigDoubleA() { return "[D"; }

	QString str() const;
private:
	JSignature(QString var);
	JSignature& addPrimitive(QString c);
	JSignature& retPrimitive(QString c);
private:
	QString m_strReturnValue;
	QString m_strArguments;
	bool m_bVariable;
};

#endif // JSIGNATURE_H
