/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2010 Lubos Dolezel <lubos a dolezel.info>

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

#include "JVM.h"

#include <dlfcn.h>

#include <QDir>
#include <QLibrary>
#include <QProcess>

#include "JAccountStatusPlugin.h"
#include "JBackgroundWorker.h"
#include "JDownloadPlugin.h"
#include "JMap.h"
#include "JObject.h"
#include "JSettings.h"
#include "JSingleCObject.h"
#include "Logger.h"
#include "Settings.h"
#include "config.h"
#ifdef POSIX_LINUX
#include <alloca.h>
#endif
#include <QtDebug>
#include <cassert>
#include <cstdlib>

JVM* JVM::m_instance = 0;

typedef jint (*cjvm_fn)(JavaVM** pvm, void** penv, void* args);

JVM::JVM(bool forceJreSearch) : m_jvm(0) {
  qRegisterMetaType<JObject>("JObject");

  QString savedPath = getSettingsValue("extensions/jvm_path").toString();
  if (forceJreSearch || savedPath.isEmpty() || !QFile::exists(savedPath)) {
    QProcess prc;

    qDebug() << "Locating the Java VM for Java-based plugins...";

    prc.start(DATA_LOCATION "/data/java/findjvm.sh", QIODevice::ReadOnly);
    prc.waitForFinished();

    int code = prc.exitCode();
    if (code == 1)
      Logger::global()->enterLogMessage(
          "JPlugin", QObject::tr("Cannot locate a Java Runtime Environment"));
    else if (code == 2)
      Logger::global()->enterLogMessage(
          "JPlugin",
          QObject::tr("Java Runtime Environment located, but no libjvm found"));
    else {
      QByteArray libname = prc.readAll().trimmed();
      jvmStartup(libname);
    }
  } else {
    qDebug() << "Loading JVM from the stored location:" << savedPath;
    jvmStartup(savedPath);
  }
}

JVM::~JVM() {
  qDebug() << "Unloading the JVM...";
  if (m_jvm) m_jvm->DestroyJavaVM();

  m_env.setLocalData(0);
  if (m_instance == this) m_instance = 0;
}

void JVM::jvmStartup(QString libname) {
  QLibrary lib(libname);
  cjvm_fn fn = (cjvm_fn)lib.resolve("JNI_CreateJavaVM");

  qDebug() << "libjvm found in" << libname;
  setSettingsValue("extensions/jvm_path", libname);

  if (!fn) {
    Logger::global()->enterLogMessage(
        "JPlugin", QObject::tr("Failed to load the correct libjvm: %1")
                       .arg(lib.errorString()));
    return;
  }

  jint res;
  JavaVMInitArgs vm_args;
#ifdef DEBUG_BUILD
  JavaVMOption options[9];
#else
  JavaVMOption options[7];
#endif

  JNIEnv* env;
  QByteArray classpath = getClassPath().toUtf8();
  int mb = getSettingsValue("java/maxheap").toInt();

  if (!mb) mb = 16;

  classpath.prepend("-Djava.class.path=");
  qDebug() << "Java Classpath set to" << classpath;

  options[0].optionString = classpath.data();
  options[1].optionString = static_cast<char*>(alloca(24));

  snprintf(options[1].optionString, 24, "-Xmx%dm", mb);
  options[2].optionString = const_cast<char*>("-Djava.security.manager");
  options[3].optionString = const_cast<char*>(
      "-Djava.security.policy=" DATA_LOCATION "/data/java/extension.policy");
  options[4].optionString = const_cast<char*>("-XX:+UseParNewGC");
  options[5].optionString = const_cast<char*>("-XX:MinHeapFreeRatio=5");
  options[6].optionString = const_cast<char*>("-XX:MaxHeapFreeRatio=10");
#ifdef DEBUG_BUILD
  options[7].optionString = const_cast<char*>("-Xdebug");
  options[8].optionString = const_cast<char*>(
      "-Xrunjdwp:transport=dt_socket,server=y,suspend=n,address=8222");
#endif

  vm_args.version = 0x00010006;
  vm_args.options = options;
  vm_args.nOptions = sizeof(options) / sizeof(options[0]);
  vm_args.ignoreUnrecognized = JNI_TRUE;

  res = fn(&m_jvm, (void**)&env, &vm_args);
  if (res < 0) {
    Logger::global()->enterLogMessage(
        "JPlugin", QObject::tr("Failed to create a Java VM"));
    return;
  }
  JNIEnv** penv = new JNIEnv*;
  *penv = env;
  m_env.setLocalData(penv);

  if (!m_instance) m_instance = this;

  try {
    singleCObjectRegisterNatives();
    JSettings::registerNatives();
    JPlugin::registerNatives();
    JTransferPlugin::registerNatives();
    JDownloadPlugin::registerNatives();
    JBackgroundWorker::registerNatives();
    JAccountStatusPlugin::registerNatives();
  } catch (...) {
    qDebug() << "Failed to register JNI functions. This usually happens when "
                "there is an API discrepancy between the Java and the native "
                "code.\nPlease, remove "
                "~/.local/share/fatrat/data/java/libs/"
                "fatrat-jplugins-core.jar, and try again";
    abort();
  }
}

QString JVM::getClassPath() {
  bool hasCore = false;
  QString rv;

  QString baseDir = QDir::homePath() + USER_PROFILE_PATH "/data/java/";
  QDir dir(baseDir);

  // JNI does not support classpath globs, we need to search for files ourselves
  QStringList list = dir.entryList(QStringList() << "*.jar", QDir::Files);

  foreach (QString f, list) {
    if (f == "fatrat-jplugins-core.jar") hasCore = true;
    if (!rv.isEmpty()) rv += ':';
    rv += dir.filePath(f);
  }

  // Now enumerate extra Java classpath libs
  dir = (DATA_LOCATION "/data/java/libs");
  list = dir.entryList(QStringList() << "*.jar", QDir::Files);
  foreach (QString f, list) {
    if (!rv.isEmpty()) rv += ':';
    if (f == "fatrat-jplugins-core.jar" && hasCore) continue;
    rv += dir.filePath(f);
  }

  return rv;
}

bool JVM::JVMAvailable() {
  if (!m_instance) return false;
  if (!m_instance->m_jvm) return false;
  return true;
}

JVM* JVM::instance() { return m_instance; }

JVM::operator JNIEnv*() {
  if (!m_jvm) return 0;

  if (!m_env.hasLocalData()) {
    // abort();

    JNIEnv* env;
    m_jvm->AttachCurrentThread((void**)&env, 0);
    JNIEnv** penv = new JNIEnv*;
    *penv = env;
    m_env.setLocalData(penv);
  }
  return *m_env.localData();
}

void JVM::detachCurrentThread() {
  assert(m_env.hasLocalData());
  m_env.setLocalData(0);
  m_jvm->DetachCurrentThread();
}

void JVM::throwException(JObject& obj) {
  JNIEnv* env = *this;
  env->Throw(static_cast<jthrowable>(obj.getLocalRef()));
}

QMap<QString, QString> JVM::getPackageVersions() {
  JClass cls("info.dolezel.fatrat.plugins.helpers.NativeHelpers");
  JMap map =
      cls.callStatic("getPackageVersions", JSignature().ret("java.util.Map"))
          .value<JObject>();
  QMap<QString, QString> rv;

  map.toQMap(rv);
  return rv;
}
