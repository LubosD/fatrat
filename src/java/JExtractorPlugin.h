/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2011 Lubos Dolezel <lubos a dolezel.info>

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


#ifndef JEXTRACTORPLUGIN_H
#define JEXTRACTORPLUGIN_H

#include "config.h"
#ifndef WITH_JPLUGINS
#	error This file is not supposed to be included!
#endif

#include "JTransferPlugin.h"
#include "engines/JavaExtractor.h"

class JExtractorPlugin : public JTransferPlugin
{
public:
	JExtractorPlugin(const JClass& cls, const char* sig = "()V", JArgs args = JArgs());
	JExtractorPlugin(const char* clsName, const char* sig = "()V", JArgs args = JArgs());

	inline void setTransfer(JavaExtractor* t) { m_transfer = t; }
	inline JavaExtractor* transfer() const { return m_transfer; }

	static void registerNatives();

	static void finishedExtraction(JNIEnv *, jobject, jobjectArray);
private:
	JavaExtractor* m_transfer;
};

#endif // JEXTRACTORPLUGIN_H
