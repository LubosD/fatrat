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

#include "JUploadPlugin.h"
#include "RuntimeException.h"
#include "JString.h"
#include "JArray.h"
#include "engines/JavaUpload.h"

JUploadPlugin::JUploadPlugin(const JClass& cls, const char* sig, JArgs args)
	: JTransferPlugin(cls, sig, args)
{
}

JUploadPlugin::JUploadPlugin(const char* clsName, const char* sig, JArgs args)
	: JTransferPlugin(clsName, sig, args)
{
}

void JUploadPlugin::registerNatives()
{
	QList<JNativeMethod> natives;

	natives << JNativeMethod("startUpload", JSignature().addString().addA("info.dolezel.fatrat.plugins.UploadPlugin$MimePart"), startUpload);
	natives << JNativeMethod("putDownloadLink", JSignature().addString().addString(), putDownloadLink);

	JClass("info.dolezel.fatrat.plugins.UploadPlugin").registerNativeMethods(natives);
}

void JUploadPlugin::startUpload(JNIEnv* env, jobject jthis, jstring jurl, jobjectArray mimeParts)
{
	JUploadPlugin* This = static_cast<JUploadPlugin*>(getCObject(jthis));
	
	QString url = JString(jurl).str();
	JArray array(mimeParts);
	QList<JavaUpload::MimePart> parts;
	
	for (size_t i = 0; i < array.size(); i++)
	{
		JMimePart part = array.getObject(i);
		JavaUpload::MimePart xpart;
		
		xpart.filePart = part.mimePartType() == JMimePart::MimePartFile;
		xpart.name = part.name();
		
		if (!xpart.filePart)
			xpart.value = part.value();
		
		parts << xpart;
	}
	
	static_cast<JavaUpload*>(This->m_transfer)->startUpload(url, parts);
}

void JUploadPlugin::putDownloadLink(JNIEnv* env, jobject jthis, jstring urlDownload, jstring killLink)
{
	JUploadPlugin* This = static_cast<JUploadPlugin*>(getCObject(jthis));
	QString klink, dlink = JString(urlDownload).str();
	
	if (killLink)
		klink = JString(killLink).str();
	static_cast<JavaUpload*>(This->m_transfer)->putDownloadLink(dlink, klink);
}

JUploadPlugin::JMimePart::JMimePart(JObject obj)
	: JObject(obj)
{
}

JUploadPlugin::JMimePart::MimePartType JUploadPlugin::JMimePart::mimePartType()
{
	if (instanceOf("info.dolezel.fatrat.plugins.UploadPlugin$MimePartValue"))
		return MimePartValue;
	if (instanceOf("info.dolezel.fatrat.plugins.UploadPlugin$MimePartFile"))
		return MimePartFile;
	
	throw RuntimeException(tr("Unknown class: %1").arg(getClass().getClassName()));
}

QString JUploadPlugin::JMimePart::name()
{
	return getValue("name", JSignature::sigString()).toString();
}

QString JUploadPlugin::JMimePart::value()
{
	return getValue("value", JSignature::sigString()).toString();
}

