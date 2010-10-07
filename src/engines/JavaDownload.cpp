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

#include "JavaDownload.h"
#include "java/JVM.h"
#include "java/JClass.h"
#include "java/JArray.h"
#include "RuntimeException.h"

#include <QApplication>
#include <QNetworkReply>
#include <QNetworkAccessManager>

QMap<QString,QMutex*> JavaDownload::m_mutexes;

JavaDownload::JavaDownload(const char* cls)
	: m_bHasLock(false)
{
	m_strClass = cls;
}

JavaDownload::~JavaDownload()
{
	if(isActive())
		changeActive(false);
}

void JavaDownload::init(QString source, QString target)
{
	if(QThread::currentThread() != QApplication::instance()->thread())
		moveToThread(QApplication::instance()->thread());
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(secondElapsed()));
}

QString JavaDownload::myClass() const
{
	return m_strClass;
}

QString JavaDownload::name() const
{
	if(isActive())
		return CurlDownload::name();
	else
		return m_strName;
}

void JavaDownload::load(const QDomNode& map)
{
	Transfer::load(map);
}

void JavaDownload::save(QDomDocument& doc, QDomNode& map) const
{
	Transfer::save(doc, map);

	setXMLProperty(doc, map, "jplugin_original", m_strOriginal);
	setXMLProperty(doc, map, "jplugin_target", m_strTarget);
	setXMLProperty(doc, map, "knowntotal", QString::number(m_nTotal));
}

void JavaDownload::changeActive(bool bActive)
{
	if (bActive)
	{

	}
	else
	{
		if(m_bHasLock)
		{
			m_mutexes[m_strClass]->unlock();
			m_bHasLock = false;
		}

		CurlDownload::changeActive(false);
		m_nSecondsLeft = -1;
		m_timer.stop();
	}
}

void JavaDownload::setObject(QString newdir)
{
	if(isActive())
		CurlDownload::setObject(newdir);
	m_strTarget = newdir;
}

qulonglong JavaDownload::done() const
{
	if(isActive())
		return CurlDownload::done();
	else if(m_state == Completed)
		return m_nTotal;
	else
		return 0; // need a Java call to confirm this
}

void JavaDownload::setState(State newState)
{
	if(newState == Transfer::Completed && done() < 10*1024)
	{
		// Java call
	}

	Transfer::setState(newState);
}

QString JavaDownload::remoteURI() const
{
	return m_strOriginal;
}

int JavaDownload::acceptable(QString uri, bool)
{

}

void JavaDownload::httpFinished(QNetworkReply* reply)
{
	reply->deleteLater();
}

void JavaDownload::secondElapsed()
{

}

void JavaDownload::deriveName()
{

}

void JavaDownload::globalInit()
{
	if (!JVM::instance())
		return;

	// search for plugins
	try
	{
		JClass mainClass("info/dolezel/fatrat/plugins/DownloadPlugin");
		JClass annotation("info/dolezel/fatrat/plugins/PluginInfo");
		QList<QVariant> args;

		args << "info.dolezel.fatrat.plugins" << annotation.toVariant();

		JArray arr = mainClass.callStatic("findAnnotatedClasses",
						  JSignature().addString().add("java.lang.Class").retA("java.lang.Class"),
						  args).value<JArray>();
		qDebug() << "Found" << arr.size() << "annotated classes";

		int classes = arr.size();
		for (int i = 0; i < classes; i++)
		{
			JClass obj = (jobject) arr.getObject(i);
			JObject ann = obj.getAnnotation(annotation);
			QString regexp = ann.call("regexp", JSignature().retString()).toString();
			QString name = ann.call("name", JSignature().retString()).toString();

			qDebug() << "Class name:" << obj.getClassName();
			qDebug() << "Name:" << name;
			qDebug() << "Regexp:" << regexp;
		}
	}
	catch (const RuntimeException& e)
	{
		qDebug() << e.what();
	}
}

void JavaDownload::globalExit()
{

}
