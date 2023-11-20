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

#ifndef JVM_H
#define JVM_H

#include "config.h"
#ifndef WITH_JPLUGINS
#error This file is not supposed to be included!
#endif

#include <jni.h>

#include <QMap>
#include <QString>
#include <QThreadStorage>

#include "JObject.h"

class JObject;

class JVM {
 public:
  JVM(bool forceJreSearch = false);
  virtual ~JVM();
  static bool JVMAvailable();
  static JVM* instance();
  operator JNIEnv*();

  QMap<QString, QString> getPackageVersions();

  void detachCurrentThread();
  void throwException(JObject& obj);

 private:
  static QString getClassPath();
  void jvmStartup(QString path);

 private:
  static JVM* m_instance;
  JavaVM* m_jvm;
  QThreadStorage<JNIEnv**> m_env;
};

#endif  // JVM_H
