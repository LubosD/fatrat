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

#include "JBackgroundWorker.h"
#include "JVM.h"
#include "JException.h"

template <> QList<JObject*> JSingleCObject<JBackgroundWorker>::m_instances = QList<JObject*>();
template <> std::unique_ptr<QReadWriteLock> JSingleCObject<JBackgroundWorker>::m_mutex = std::unique_ptr<QReadWriteLock>(new QReadWriteLock);


JBackgroundWorker::JBackgroundWorker(jobject jthis)
	: JObject(jthis)
{
	connect(this, SIGNAL(finished()), this, SLOT(finished()));
}

void JBackgroundWorker::registerNatives()
{
	QList<JNativeMethod> natives;

	natives << JNativeMethod("get", JSignature().ret("java.lang.Object"), get);
	natives << JNativeMethod("execute", JSignature(), execute);
	natives << JNativeMethod("disposeNative", JSignature(), disposeNative);

	JClass("info.dolezel.fatrat.plugins.util.BackgroundWorker").registerNativeMethods(natives);
}

void JBackgroundWorker::execute(JNIEnv *, jobject jthis)
{
	JBackgroundWorker* This = getCObjectAuto(jthis);
	This->run();
}

jobject JBackgroundWorker::get(JNIEnv *, jobject jthis)
{
	JBackgroundWorker* This = getCObjectAuto(jthis);

	This->wait();

	if (This->m_exception.isNull())
		return This->m_result.getLocalRef();
	else
	{
		JObject ex("java.util.concurrent.ExecutionException", JSignature().add("java.lang.Throwable"), JArgs() << This->m_exception);
		JVM::instance()->throwException(ex);
		return 0;
	}
}

void JBackgroundWorker::updateProgress(JNIEnv*, jobject jthis, jobject p)
{
	QMetaObject::invokeMethod(getCObjectAuto(jthis), "progressUpdated", Q_ARG(JObject, JObject(p)));
}

void JBackgroundWorker::progressUpdated(JObject p)
{
	call("progressUpdated", JSignature().add("java.lang.Object"), JArgs() << p);
}

void JBackgroundWorker::disposeNative(JNIEnv *, jobject jthis)
{
	delete getCObject(jthis);
}

void JBackgroundWorker::finished()
{
	call("done");
}

void JBackgroundWorker::run()
{
	try
	{
		m_result = call("doInBackground", JSignature().ret("java.lang.Object")).value<JObject>();
	}
	catch (const JException& e)
	{
		m_exception = e.javaObject();
	}

	JVM::instance()->detachCurrentThread();
}
