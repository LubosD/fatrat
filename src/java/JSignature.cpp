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

#include "JSignature.h"

JSignature::JSignature()
	: m_strReturnValue("V"), m_bVariable(false)
{
}

JSignature::JSignature(QString var)
	: m_bVariable(true)
{
	m_strReturnValue = var;
}

JSignature& JSignature::add(QString cls)
{
	cls.replace('.', '/');
	m_strArguments += 'L'+cls+';';
	return *this;
}

JSignature& JSignature::addA(QString cls)
{
	cls.replace('.', '/');
	m_strArguments += "[L"+cls+';';
	return *this;
}

JSignature& JSignature::addPrimitive(QString c)
{
	m_strArguments += c;
	return *this;
}


JSignature& JSignature::ret(QString cls)
{
	cls.replace('.', '/');
	m_strReturnValue =  'L'+cls+';';
	return *this;
}

JSignature& JSignature::retA(QString cls)
{
	cls.replace('.', '/');
	m_strReturnValue =  "[L"+cls+';';
	return *this;
}

JSignature& JSignature::retPrimitive(QString c)
{
	m_strReturnValue = c;
	return *this;
}

QString JSignature::str() const
{
	if (!m_bVariable)
		return "("+m_strArguments+")"+m_strReturnValue;
	else
		return m_strReturnValue;
}

JSignature JSignature::sig(QString cls)
{
	QString name = "L" + cls.replace('.', '/')+';';
	return JSignature(name);
}

JSignature JSignature::sigA(QString cls)
{
	QString name = "[L" + cls.replace('.', '/')+';';
	return JSignature(name);
}

