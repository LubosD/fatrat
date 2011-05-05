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

#include "JDownloadPlugin.h"
#include "JSettings.h"
#include "JMap.h"
#include "Settings.h"
#include "config.h"
#include <alloca.h>

#include <QtDebug>

JVM* JVM::m_instance = 0;

typedef jint (*cjvm_fn) (JavaVM **pvm, void **penv, void *args);

JVM::JVM(bool forceJreSearch) : m_jvm(0)
{
	QString savedPath = getSettingsValue("extensions/jvm_path").toString();
	if (forceJreSearch || savedPath.isEmpty() || !QFile::exists(savedPath))
	{
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
			jvmStartup(libname);
		}
	}
	else
	{
		qDebug() << "Loading JVM from the stored location:" << savedPath;
		jvmStartup(savedPath);
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

void JVM::jvmStartup(QString libname)
{
	QLibrary lib (libname);
	cjvm_fn fn = (cjvm_fn) lib.resolve("JNI_CreateJavaVM");

	qDebug() << "libjvm found in" << libname;
	setSettingsValue("extensions/jvm_path", libname);

	if (!fn)
	{
		Logger::global()->enterLogMessage("JPlugin", QObject::tr("Failed to load the correct libjvm: %1").arg(lib.errorString()));
		return;
	}

	jint res;
	JavaVMInitArgs vm_args;
	JavaVMOption options[2];
	JNIEnv* env;
	QByteArray classpath = getClassPath().toUtf8();
	int mb = getSettingsValue("java/maxheap").toInt();

	if (!mb)
		mb = 16;

	classpath.prepend("-Djava.class.path=");
	qDebug() << "Java Classpath set to" << classpath;

	options[0].optionString = classpath.data();
	options[1].optionString = static_cast<char*>(alloca(24));

	snprintf(options[1].optionString, 24, "-Xmx%dm", mb);

	vm_args.version = 0x00010006;
	vm_args.options = options;
	vm_args.nOptions = sizeof(options)/sizeof(options[0]);
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

	if (!m_instance)
		m_instance = this;

	JSettings::registerNatives();
	JPlugin::registerNatives();
	JTransferPlugin::registerNatives();
	JDownloadPlugin::registerNatives();
}

QString JVM::getClassPath()
{
	bool hasCore = false;
	QString rv;

	QString baseDir = QDir::homePath() + USER_PROFILE_PATH "/data/java/";
	QDir dir(baseDir);

	// JNI does not support classpath globs, we need to search for files ourselves
	QStringList list = dir.entryList(QStringList() << "*.jar", QDir::Files);

	foreach (QString f, list)
	{
		if (f == "fatrat-jplugins.jar")
			hasCore = true;
		if (!rv.isEmpty())
			rv += ':';
		rv += dir.filePath(f);
	}

	if (!hasCore)
		rv += ":" DATA_LOCATION "/data/java/fatrat-jplugins.jar";

	// Now enumerate extra Java classpath libs
	dir = (DATA_LOCATION "/data/java/libs");
	list = dir.entryList(QStringList() << "*.jar", QDir::Files);
	foreach (QString f, list)
	{
		if (!rv.isEmpty())
			rv += ':';
		rv += dir.filePath(f);
	}

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

QMap<QString,QString> JVM::getPackageVersions()
{
	JClass cls("info.dolezel.fatrat.plugins.helpers.NativeHelpers");
	JMap map = cls.callStatic("getPackageVersions", JSignature().ret("java.util.Map")).value<JObject>();
	QMap<QString,QString> rv;

	map.toQMap(rv);
	return rv;
}
