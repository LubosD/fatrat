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

#include "JVM.h"
#include <dlfcn.h>
#include <QProcess>
#include "Logger.h"
#include <QLibrary>
#include <QDir>

#include "info_dolezel_fatrat_plugins_FRDownloadPlugin.h"

#include <QtDebug>

extern const char* USER_PROFILE_PATH;
JVM* JVM::m_instance = 0;

typedef jint (*cjvm_fn) (JavaVM **pvm, void **penv, void *args);

JVM::JVM() : m_jvm(0)
{
	if (!m_instance)
		m_instance = 0;

	QProcess prc;

	qDebug() << "Locating the Java VM for Java-based plugins...";

	prc.start(DATA_LOCATION "/data/java/findjvm.sh", QIODevice::ReadOnly);
	prc.waitForFinished();

	int code = prc.exitCode();
	if (code == 1)
		Logger::global()->enterLogMessage("JPlugin", QObject::tr("Cannot locate a Java Runtime Environment"));
	else if (code == 2)
		Logger::global()->enterLogMessage("JPlugin", QObject::tr("Java Runtime Environment located, but no libjvm found"));
	else
	{
		QByteArray libname = prc.readAll().trimmed();
		QLibrary lib (libname);
		cjvm_fn fn = (cjvm_fn) lib.resolve("JNI_CreateJavaVM");

		qDebug() << "libjvm found in" << libname;

		if (!fn)
		{
			Logger::global()->enterLogMessage("JPlugin", QObject::tr("Failed to load the correct libjvm: %1").arg(lib.errorString()));
			return;
		}

		jint res;
		JavaVMInitArgs vm_args;
		JavaVMOption options[1];
		JNIEnv* env;
		QByteArray cp = getClassPath().toUtf8();
		char opt[1024] = "-Djava.class.path=";

		strcat(opt, cp.constData());

		qDebug() << "Java Classpath set to" << cp;

		options[0].optionString = opt;

		vm_args.version = 0x00010002;
		vm_args.options = options;
		vm_args.nOptions = 1;
		vm_args.ignoreUnrecognized = JNI_TRUE;

		res = fn(&m_jvm, (void**)&env, &vm_args);
		if (res < 0)
		{
			Logger::global()->enterLogMessage("JPlugin", QObject::tr("Failed to create a Java VM"));
			return;
		}
		JNIEnv** penv = new JNIEnv*;
		*penv = env;
		m_env.setLocalData(penv);

		registerNatives();
	}
}

JVM::~JVM()
{
	qDebug() << "Unloading the JVM...";
	if (m_jvm)
		m_jvm->DestroyJavaVM();
	if (m_instance == this)
		m_instance = 0;
}

void JVM::registerNatives()
{
	JNIEnv* env = (JNIEnv*) *this;
	jclass cls = env->FindClass("info/dolezel/fatrat/plugins/FRDownloadPlugin");
	if (!cls)
	{
		Logger::global()->enterLogMessage("JPlugin", QObject::tr("Failed to register native methods"));
		return;
	}

	JNINativeMethod nm[6];

	nm[0].name = const_cast<char*>("setMessage");
	nm[0].signature = const_cast<char*>("(Ljava/lang/String;)V");
	nm[0].fnPtr = reinterpret_cast<void*>(Java_info_dolezel_fatrat_plugins_FRDownloadPlugin_setMessage);

	nm[1].name = const_cast<char*>("setState");
	nm[1].signature = const_cast<char*>("(Linfo/dolezel/fatrat/plugins/FRDownloadPlugin/State;)V");
	nm[1].fnPtr = reinterpret_cast<void*>(Java_info_dolezel_fatrat_plugins_FRDownloadPlugin_setState);

	nm[2].name = const_cast<char*>("fetchPage");
	nm[2].signature = const_cast<char*>("(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
	nm[2].fnPtr = reinterpret_cast<void*>(Java_info_dolezel_fatrat_plugins_FRDownloadPlugin_fetchPage);

	nm[3].name = const_cast<char*>("startDownload");
	nm[3].signature = const_cast<char*>("(Ljava/lang/String;)V");
	nm[3].fnPtr = reinterpret_cast<void*>(Java_info_dolezel_fatrat_plugins_FRDownloadPlugin_startDownload);

	nm[4].name = const_cast<char*>("startWait");
	nm[4].signature = const_cast<char*>("(ILjava/lang/String;)V");
	nm[4].fnPtr = reinterpret_cast<void*>(Java_info_dolezel_fatrat_plugins_FRDownloadPlugin_startWait);

	nm[5].name = const_cast<char*>("logMessage");
	nm[5].signature = const_cast<char*>("(Ljava/lang/String;)V");
	nm[5].fnPtr = reinterpret_cast<void*>(Java_info_dolezel_fatrat_plugins_FRDownloadPlugin_logMessage);

	env->RegisterNatives(cls, nm, 6);
}

QString JVM::getClassPath()
{
	QString rv;
	rv = DATA_LOCATION "/data/java/fatrat-jplugins.jar:";
	rv += QDir::homePath() + QLatin1String(USER_PROFILE_PATH) + "/data/java/*.jar";

	return rv;
}

bool JVM::JVMAvailable()
{
	if (!m_instance)
		return false;
	if (!m_instance->m_jvm)
		return false;
	return true;
}

JVM* JVM::instance()
{
	return m_instance;
}

JVM::operator JNIEnv*()
{
	if (!m_jvm)
		return 0;

	if (!m_env.hasLocalData())
	{
		JNIEnv *env;
		m_jvm->AttachCurrentThread((void **)&env, 0);
		JNIEnv** penv = new JNIEnv*;
		*penv = env;
		m_env.setLocalData(penv);
	}
	return *m_env.localData();
}
