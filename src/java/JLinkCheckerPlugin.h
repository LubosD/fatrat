/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2012 Lubos Dolezel <lubos a dolezel.info>

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

#ifndef JLINKCHECKERPLUGIN_H
#define JLINKCHECKERPLUGIN_H

#include "config.h"
#ifndef WITH_JPLUGINS
#error This file is not supposed to be included!
#endif

#include <QStringList>

#include "JPlugin.h"

class JLinkCheckerPlugin : public JPlugin {
  Q_OBJECT
 public:
  JLinkCheckerPlugin(const JClass& cls, const char* sig = "()V",
                     JArgs args = JArgs());
  JLinkCheckerPlugin(const char* clsName, const char* sig = "()V",
                     JArgs args = JArgs());

  static void registerNatives();

 protected:
  static void reportBroken(JNIEnv*, jobject, jarray);
  static void reportWorking(JNIEnv*, jobject, jarray);
  static void reportDone(JNIEnv*, jobject);
 signals:
  void reportedBroken(QStringList);
  void reportedWorking(QStringList);
  void done();

 private:
  // QStringList m_working, m_broken;
};

#endif  // JLINKCHECKERPLUGIN_H
