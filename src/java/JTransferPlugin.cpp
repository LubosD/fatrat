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

#include "JTransferPlugin.h"

#include "JException.h"
#include "JMap.h"
#include "JString.h"
#include "engines/JavaDownload.h"
#include "engines/StaticTransferMessage.h"

JTransferPlugin::JTransferPlugin(const JClass& cls, const char* sig, JArgs args)
    : JPlugin(cls, sig, args) {}

JTransferPlugin::JTransferPlugin(const char* clsName, const char* sig,
                                 JArgs args)
    : JPlugin(clsName, sig, args) {}

void JTransferPlugin::abort() { JPlugin::abort(); }

void JTransferPlugin::registerNatives() {
  QList<JNativeMethod> natives;

  natives << JNativeMethod("setMessage", JSignature().addString(), setMessage);
  natives << JNativeMethod(
      "setState",
      JSignature().add("info.dolezel.fatrat.plugins.TransferPlugin$State"),
      setState);
  natives << JNativeMethod("logMessage", JSignature().addString(), logMessage);
  natives << JNativeMethod("setPersistentVariable",
                           JSignature().addString().add("java.lang.Object"),
                           setPersistentVariable);
  natives << JNativeMethod("getPersistentVariable",
                           JSignature().addString().ret("java.lang.Object"),
                           getPersistentVariable);

  JClass("info.dolezel.fatrat.plugins.TransferPlugin")
      .registerNativeMethods(natives);
}

void JTransferPlugin::setMessage(JNIEnv* env, jobject jthis, jstring msg) {
  JPlugin* plugin = getCObject(jthis);
  plugin->transfer()->setMessage(JString(msg));
}

void JTransferPlugin::setState(JNIEnv* env, jobject jthis, jobject state) {
  JStateEnum e(state);
  getCObject(jthis)->transfer()->setState(e.value());
}

void JTransferPlugin::logMessage(JNIEnv* env, jobject jthis, jstring msg) {
  getCObject(jthis)->transfer()->enterLogMessage(JString(msg));
}

JTransferPlugin::JStateEnum::JStateEnum(jobject obj) : JObject(obj) {}

Transfer::State JTransferPlugin::JStateEnum::value() const {
  int state = const_cast<JStateEnum*>(this)
                  ->call("value", JSignature().retInt())
                  .toInt();
  return Transfer::State(state);
}

void JTransferPlugin::setPersistentVariable(JNIEnv* env, jobject jthis,
                                            jstring jkey, jobject jval) {
  JString key(jkey);
  QVariant var;
  JObject obj(jval);

  JMap::boxedToNative(obj, var);

  static_cast<JTransferPlugin*>(getCObject(jthis))
      ->setPersistentVariable(key.str(), var);
}

jobject JTransferPlugin::getPersistentVariable(JNIEnv* env, jobject jthis,
                                               jstring key) {
  QVariant var = static_cast<JTransferPlugin*>(getCObject(jthis))
                     ->getPersistentVariable(JString(key).str());
  return JMap::nativeToBoxed(var).getLocalRef();
}
