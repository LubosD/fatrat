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

#ifndef JBACKGROUNDOWORKER_H
#define JBACKGROUNDOWORKER_H
#include <QThread>
#include "JObject.h"
#include "JSingleCObject.h"

class JBackgroundWorker : public QThread, public JObject, public JSingleCObject<JBackgroundWorker>
{
Q_OBJECT
public:
	JBackgroundWorker(jobject jthis, bool weak);

	virtual void run();

	static void registerNatives();

	static void execute(JNIEnv *, jobject);
	static jobject get(JNIEnv *, jobject);
	static void updateProgress(JNIEnv*, jobject, jobject);
private slots:
	void finished();
	void progressUpdated(JObject p);
private:
	JObject m_result;
	JObject m_exception;
};

#endif
