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

#include "JExtractorPlugin.h"

#include <QList>
#include <QString>
#include <QtDebug>

#include "JArray.h"
#include "JClass.h"
#include "JString.h"

JExtractorPlugin::JExtractorPlugin(const JClass& cls, const char* sig,
                                   JArgs args)
    : JTransferPlugin(cls, sig, args) {}

JExtractorPlugin::JExtractorPlugin(const char* clsName, const char* sig,
                                   JArgs args)
    : JTransferPlugin(clsName, sig, args) {}

void JExtractorPlugin::registerNatives() {
  QList<JNativeMethod> natives;

  natives << JNativeMethod("finishedExtraction", JSignature().addStringA(),
                           finishedExtraction);

  JClass("info.dolezel.fatrat.plugins.ExtractorPlugin")
      .registerNativeMethods(natives);
}

void JExtractorPlugin::finishedExtraction(JNIEnv* env, jobject jthis,
                                          jobjectArray jlinks) {
  JExtractorPlugin* This = static_cast<JExtractorPlugin*>(getCObject(jthis));
  JArray links(jlinks);

  QList<QString> list;
  qDebug() << list;
  for (size_t i = 0; i < links.size(); i++) {
    JString str = links.getObject(i).toStringShallow();
    list << str.str();
  }

  static_cast<JavaExtractor*>(This->m_transfer)->finishedExtraction(list);
  This->m_bTaskDone = true;
}

void JExtractorPlugin::setPersistentVariable(QString key, QVariant value) {
  static_cast<JavaExtractor*>(transfer())->setPersistentVariable(key, value);
}

QVariant JExtractorPlugin::getPersistentVariable(QString key) {
  return static_cast<JavaExtractor*>(transfer())->getPersistentVariable(key);
}
