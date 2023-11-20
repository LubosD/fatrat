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

#ifndef JUPLOADPLUGIN_H
#define JUPLOADPLUGIN_H

#include "config.h"
#ifndef WITH_JPLUGINS
#error This file is not supposed to be included!
#endif

#include "JTransferPlugin.h"
#include "Transfer.h"

class JavaUpload;

class JUploadPlugin : public JTransferPlugin {
 public:
  JUploadPlugin(const JClass& cls, const char* sig = "()V",
                JArgs args = JArgs());
  JUploadPlugin(const char* clsName, const char* sig = "()V",
                JArgs args = JArgs());

  static void registerNatives();

  // JNI methods
  static void startUploadChunk(JNIEnv*, jobject, jstring, jobjectArray, jlong,
                               jlong);
  static void putDownloadLink(JNIEnv*, jobject, jstring, jstring);

  virtual void setPersistentVariable(QString key, QVariant value);
  virtual QVariant getPersistentVariable(QString key);

  class JMimePart : public JObject {
   public:
    JMimePart(JObject obj);
    enum MimePartType { MimePartValue, MimePartFile };

    MimePartType mimePartType();
    QString name();
    QString value();
  };

 private:
};

#endif
