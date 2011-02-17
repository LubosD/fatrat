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

#include "JPlugin.h"
#include <QNetworkCookieJar>
#include "JString.h"
#include "JByteBuffer.h"
#include "JMap.h"
#include "JException.h"
#include "engines/JavaDownload.h"

template <> QList<JObject*> JSingleCObject<JPlugin>::m_instances = QList<JObject*>();
template <> std::auto_ptr<QReadWriteLock> JSingleCObject<JPlugin>::m_mutex = std::auto_ptr<QReadWriteLock>(new QReadWriteLock);


JPlugin::JPlugin(const JClass& cls, const char* sig, JArgs args)
	: JObject(cls, sig, args), m_transfer(0)
{
	setCObject();
	m_network = new QNetworkAccessManager(this);
	m_network->setCookieJar(new QNetworkCookieJar(m_network));
	connect(m_network, SIGNAL(finished(QNetworkReply*)), this, SLOT(fetchFinished(QNetworkReply*)));
}

JPlugin::JPlugin(const char* clsName, const char* sig, JArgs args)
		: JObject(clsName, sig, args), m_transfer(0)
{
	setCObject();
	m_network = new QNetworkAccessManager(this);
	m_network->setCookieJar(new QNetworkCookieJar(m_network));
	connect(m_network, SIGNAL(finished(QNetworkReply*)), this, SLOT(fetchFinished(QNetworkReply*)));
}


void JPlugin::abort()
{
	for(QMap<QNetworkReply*,RequesterReceiver>::iterator it = m_fetchCallbacks.begin(); it != m_fetchCallbacks.end();)
	{
		if (it.value().first == this)
			it = m_fetchCallbacks.erase(it);
		else
			it++;
	}
}

void JPlugin::registerNatives()
{
	QList<JNativeMethod> natives;

	natives << JNativeMethod("fetchPage", JSignature().addString().add("info.dolezel.fatrat.plugins.listeners.PageFetchListener").addString(), fetchPage);

	JClass("info.dolezel.fatrat.plugins.Plugin").registerNativeMethods(natives);
}

void JPlugin::fetchPage(JNIEnv* env, jobject jthis, jstring jurl, jobject cbInterface, jstring postData)
{
	JPlugin* This = getCObject(jthis);
	QString url = JString(jurl).toString();
	QNetworkReply* reply;

	This->m_transfer->logMessage(QLatin1String("JPlugin::fetchPage(): ")+url);
	qDebug() << "JPlugin::fetchPage():" << url;
	if (postData)
		qDebug() << "postData:" << JString(postData).str();

	if (!postData)
		reply = This->m_network->get(QNetworkRequest(url));
	else
	{
		QByteArray pd = JString(postData).str().toUtf8();
		reply = This->m_network->post(QNetworkRequest(url), pd);
	}

	This->m_fetchCallbacks[reply] = RequesterReceiver(This, cbInterface);
}

void JPlugin::fetchFinished(QNetworkReply* reply)
{
	reply->deleteLater();

	if (!m_fetchCallbacks.contains(reply))
		return; // The transfer has been stopped

	JObject& iface = m_fetchCallbacks[reply].second;

	try
	{
		if (reply->error() != QNetworkReply::NoError)
		{
			m_transfer->logMessage(QLatin1String("JPlugin::fetchFinished(): ")+reply->errorString());
			iface.call("onFailed", JSignature().addString(), reply->errorString());
		}
		else
		{
			QByteArray qbuf = reply->readAll();
			JByteBuffer buf (qbuf.data(), qbuf.size());

			QList<QByteArray> list = reply->rawHeaderList();
			JMap map(list.size());

			foreach (QByteArray ba, list)
			{
				QString k, v;

				k = QString(ba).toLower();
				v = QString(reply->rawHeader(ba)).trimmed();
				qDebug() << "Header:" << k << v;
				map.put(k, v);
			}

			qDebug() << "fetchFinished.onCompleted:" << buf.toString();
			m_transfer->logMessage(QLatin1String("JPlugin::fetchFinished(): OK"));

			iface.call("onCompleted", JSignature().add("java.nio.ByteBuffer").add("java.util.Map"), buf, map);
		}
	}
	catch (const JException& e)
	{
		m_transfer->setMessage(tr("Java exception: %1").arg(e.what()));
		m_transfer->setState(Transfer::Failed);
	}

	m_fetchCallbacks.remove(reply);
}
