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

#include "JSearchPlugin.h"

#include "JArray.h"
#include "tools/FileSharingSearch.h"

JSearchPlugin::JSearchPlugin(const JClass& cls, const char* sig, JArgs args)
    : JPlugin(cls, sig, args), m_dialog(0) {}

JSearchPlugin::JSearchPlugin(const char* clsName, const char* sig, JArgs args)
    : JPlugin(clsName, sig, args), m_dialog(0) {}

void JSearchPlugin::registerNatives() {
  QList<JNativeMethod> natives;

  natives << JNativeMethod(
      "searchDone",
      JSignature().addA(
          "info.dolezel.fatrat.plugins.SearchPlugin$SearchResult"),
      searchDone);

  JClass("info.dolezel.fatrat.plugins.SearchPlugin")
      .registerNativeMethods(natives);
}

/*
void JSearchPlugin::findMyName()
{
        JObject ann =
this->getClass().getAnnotation("info.dolezel.fatrat.plugins.annotations.SearchPluginInfo");
        if (!ann.isNull())
        {
                m_strMyName = ann.call("name",
JSignature().retString()).toString();
        }
}
*/

void JSearchPlugin::searchDone(JNIEnv*, jobject jthis, jobjectArray sr) {
  JSearchPlugin* This = static_cast<JSearchPlugin*>(getCObject(jthis));

  if (!sr) {
    // failed
    This->m_dialog->searchFailed(This->getClass().getClassName());
  } else {
    JArray results(sr);
    QList<FileSharingSearch::SearchResult> srs;

    for (unsigned int i = 0; i < results.length(); i++) {
      JObject obj = results.getObject(i);
      FileSharingSearch::SearchResult sr;

      sr.name = obj.getValue("name", JSignature::sigString()).toString();
      sr.url = obj.getValue("url", JSignature::sigString()).toString();
      sr.extraInfo =
          obj.getValue("extraInfo", JSignature::sigString()).toString();
      sr.fileSize =
          obj.getValue("fileSize", JSignature::sigLong()).toLongLong();

      srs << sr;
    }

    This->m_dialog->addSearchResults(This->getClass().getClassName(), srs);
  }
  This->m_bTaskDone = true;
}
