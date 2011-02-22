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

#include "JavaExtractor.h"
#include "java/JClass.h"
#include "java/JObject.h"
#include "java/JArray.h"
#include "RuntimeException.h"
#include "TransferFactory.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "java/JExtractorPlugin.h"
#include "java/JByteBuffer.h"
#include "java/JMap.h"
#include <cassert>
#include <QtDebug>

QMap<QString,JavaExtractor::JavaEngine> JavaExtractor::m_engines;

JavaExtractor::JavaExtractor(const char* clsName)
	: m_strClass(clsName), m_plugin(0), m_reply(0)
{
	m_network = new QNetworkAccessManager(this);
	// TODO: set proxy
	connect(m_network, SIGNAL(finished(QNetworkReply*)), this, SLOT(finished(QNetworkReply*)));

	m_plugin = new JExtractorPlugin(clsName);
	m_plugin->setTransfer(this);
}

JavaExtractor::~JavaExtractor()
{
	delete m_plugin;
}

int JavaExtractor::acceptable(QString uri, bool, const EngineEntry* e)
{
	qDebug() << "JavaExtractor::acceptable():" << uri << e->shortName;
	assert(m_engines.contains(e->shortName));

	JavaEngine& en = m_engines[e->shortName];

	if (!en.ownAcceptable.isNull())
	{
		try
		{
			return en.ownAcceptable.call("acceptable", JSignature().addString().retInt(), JArgs() << uri).toInt();
		}
		catch (RuntimeException& e)
		{
			Logger::global()->enterLogMessage("JavaExtractor", QString("%1 threw an exception in acceptable(): %2")
							  .arg(QString::fromStdString(en.name)).arg(e.what()));
		}
	}
	else if (en.regexp.exactMatch(uri))
		return 3;

	return 0;
}

void JavaExtractor::globalInit()
{
	JClass helper("info.dolezel.fatrat.plugins.helpers.NativeHelpers");
	JClass annotation("info.dolezel.fatrat.plugins.annotations.ExtractorPluginInfo");

	JArgs args;

	args << "info.dolezel.fatrat.plugins" << annotation.toVariant();

	JArray arr = helper.callStatic("findAnnotatedClasses",
					  JSignature().addString().add("java.lang.Class").retA("java.lang.Class"),
					  args).value<JArray>();
	qDebug() << "Found" << arr.size() << "annotated classes";

	int classes = arr.size();
	for (int i = 0; i < classes; i++)
	{
		try
		{
			JClass obj = (jobject) arr.getObject(i);
			JObject ann = obj.getAnnotation(annotation);
			QString regexp = ann.call("regexp", JSignature().retString()).toString();
			QString name = ann.call("name", JSignature().retString()).toString();
			QString targetClassName = ann.call("transferClassName", JSignature().retString()).toString();
			QString clsName = obj.getClassName();
			JObject instance(obj, JSignature());

			if (targetClassName.isEmpty())
			{
				JClass cls = JClass(ann.call("transferClass", JSignature().ret("java.lang.Class")).value<JObject>());
				if (!cls.isNull())
					targetClassName = cls.getClassName();
			}

			qDebug() << "Class name:" << clsName;
			qDebug() << "Name:" << name;
			qDebug() << "Regexp:" << regexp;

			JavaEngine e = { name.toStdString(), clsName.toStdString(), QRegExp(regexp) };

			if (instance.instanceOf("info.dolezel.fatrat.plugins.extra.URLAcceptableFilter"))
				e.ownAcceptable = instance;
			e.targetClass = targetClassName;

			m_engines[name] = e;

			EngineEntry entry;
			entry.longName = m_engines[clsName].name.c_str();
			entry.shortName = m_engines[clsName].shortName.c_str();
			entry.lpfnAcceptable2 = JavaExtractor::acceptable;
			entry.lpfnCreate2 = JavaExtractor::createInstance;
			entry.lpfnInit = 0;
			entry.lpfnExit = 0;
			entry.lpfnMultiOptions = 0;

			addTransferClass(entry, Transfer::Download);
		}
		catch (const RuntimeException& e)
		{
			qDebug() << e.what();
		}
	}
}

QString JavaExtractor::name() const
{
	return QString::fromStdString(m_engines[m_strClass].name);
}

void JavaExtractor::init(QString source, QString target)
{
	m_strUrl = source;
	m_strTarget = target;
}

void JavaExtractor::load(const QDomNode& map)
{
	m_strUrl = getXMLProperty(map, "jplugin_url");
	m_strTarget = getXMLProperty(map, "jplugin_target");

	Transfer::load(map);
}

void JavaExtractor::save(QDomDocument& doc, QDomNode& map) const
{
	Transfer::save(doc, map);

	setXMLProperty(doc, map, "jplugin_url", m_strUrl);
	setXMLProperty(doc, map, "jplugin_target", m_strTarget);
}

void JavaExtractor::changeActive(bool nowActive)
{
	if (nowActive)
	{
		m_reply = m_network->get(QNetworkRequest(m_strUrl));
	}
	else
	{
		if (m_reply)
			m_reply->abort();
	}
}

void JavaExtractor::finishedExtraction(QList<QString> list)
{
	if (list.isEmpty())
	{
		setMessage(tr("Empty or invalid link list"));
		setState(Failed);
		return;
	}

	QString clsName = m_engines[m_strClass].targetClass;
	if (clsName.isEmpty())
	{
		BestEngine e = bestEngine(list[0], Download);
		if (!e.engine)
		{
			setMessage(tr("Failed to detect URL class: %1").arg(list[0]));
			setState(Failed);
			return;
		}

		clsName = e.engine->shortName;
	}

	QList<Transfer*> ts;
	TransferFactory* f = TransferFactory::instance();
	foreach (QString url, list)
	{
		Transfer *t = f->createInstance(clsName);
		t->init(url, m_strTarget);
		t->setState(Waiting);
		ts << t;
	}

	replaceItself(ts);
}

void JavaExtractor::finished(QNetworkReply* reply)
{
	m_reply = 0;
	reply->deleteLater();

	if (reply->error() != QNetworkReply::NoError)
	{
		setMessage(reply->errorString());
		setState(Failed);
		return;
	}

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

	qDebug() << "JavaExtractor::finished:" << buf.toString();
	logMessage(QLatin1String("JavaExtractor::finished(): OK"));

	m_plugin->call("extractList", JSignature().addString().add("java.nio.ByteBuffer").add("java.util.Map"), m_strUrl, buf, map);
}
