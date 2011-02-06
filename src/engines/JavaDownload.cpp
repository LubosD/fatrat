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
#include "java/JException.h"
#include "RuntimeException.h"
#include "java/JDownloadPlugin.h"
#include "fatrat.h"
#include "Settings.h"
#include "SettingsJavaPluginForm.h"

#include <QApplication>
#include <QMessageBox>
#include <QtDebug>
#include <cassert>

QMap<QString,QMutex*> JavaDownload::m_mutexes;
QMap<QString,JavaDownload::JavaEngine> JavaDownload::m_engines;

JavaDownload::JavaDownload(const char* cls)
	: m_bHasLock(false), m_plugin(0)
{
	qDebug() << "JavaDownload::JavaDownload(): " << cls;
	assert(m_engines.contains(cls));

	m_strClass = cls;
	m_plugin = new JDownloadPlugin(cls);
	m_plugin->setTransfer(this);
}

JavaDownload::~JavaDownload()
{
	if(isActive())
		changeActive(false);

	delete m_plugin;
}

void JavaDownload::init(QString source, QString target)
{
	m_strOriginal = source;
	m_strTarget = target;
	deriveName();
}

QString JavaDownload::myClass() const
{
	return m_strClass;
}

QString JavaDownload::name() const
{
	if(!m_downloadUrl.isEmpty())
		return CurlDownload::name();
	else
		return m_strName;
}

void JavaDownload::load(const QDomNode& map)
{
	m_strOriginal = getXMLProperty(map, "jplugin_original");
	m_strTarget = getXMLProperty(map, "jplugin_target");
	m_strName = getXMLProperty(map, "jplugin_name");
	m_nTotal = getXMLProperty(map, "knowntotal").toLongLong();

	if (m_strName.isEmpty())
		deriveName();

	Transfer::load(map);
}

void JavaDownload::save(QDomDocument& doc, QDomNode& map) const
{
	Transfer::save(doc, map);

	setXMLProperty(doc, map, "jplugin_original", m_strOriginal);
	setXMLProperty(doc, map, "jplugin_target", m_strTarget);
	setXMLProperty(doc, map, "jplugin_name", m_strName);
	setXMLProperty(doc, map, "knowntotal", QString::number(m_nTotal));
}

void JavaDownload::changeActive(bool bActive)
{
	if (bActive)
	{
		try
		{
			QString clsName = m_plugin->getClass().getClassName();
			if (m_engines[clsName].forceSingleTransfer)
			{
				if (!m_mutexes[m_strClass])
					m_mutexes[m_strClass] = new QMutex;

				if (!m_bHasLock && !m_mutexes[m_strClass]->tryLock())
				{
					enterLogMessage(m_strMessage = tr("You cannot have multiple active transfers from this server."));
					setState(Failed);
					return;
				}
				else
					m_bHasLock = true;
			}

			if (m_engines[clsName].truncate)
			{
				Segment s;

				s.offset = s.bytes = 0;
				s.urlIndex = 0;
				s.client = 0;

				m_segments.clear();
				m_segments << s;
			}

			assert(!m_strOriginal.isEmpty());

			m_plugin->call("processLink", JSignature().addString(), JArgs() << m_strOriginal);
		}
		catch (const RuntimeException& e)
		{
			setMessage(tr("Java exception: %1").arg(e.what()));
			setState(Failed);
		}
	}
	else
	{
		if(m_bHasLock)
		{
			assert(m_mutexes.contains(m_strClass));
			m_mutexes[m_strClass]->unlock();
			m_bHasLock = false;
		}

		if (!m_downloadUrl.isEmpty())
			m_strName = CurlDownload::name();

		if (m_state == Failed)
			m_plugin->call("onFailed");

		m_downloadUrl = QUrl();
		m_plugin->abort();

		CurlDownload::changeActive(false);
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
	QString clsName = m_plugin->getClass().getClassName();

	if(isActive() || (!m_engines[clsName].truncate && !m_segments.isEmpty()))
		return CurlDownload::done();
	else if(m_state == Completed)
		return m_nTotal;
	else
		return 0;
}

void JavaDownload::setState(State newState)
{
	if(newState == Transfer::Completed)
	{
		try
		{
			m_plugin->call("finalCheck", JSignature().addString(), JArgs() << dataPath(true));
		}
		catch (const JException& e)
		{
			setMessage(tr("Java exception: %1").arg(e.what()));
			setState(Failed);
		}
	}

	Transfer::setState(newState);
}

QString JavaDownload::remoteURI() const
{
	return m_strOriginal;
}

int JavaDownload::acceptable(QString uri, bool, const EngineEntry* e)
{
	qDebug() << "JavaDownload::acceptable():" << uri << e->shortName;
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
			Logger::global()->enterLogMessage("JavaDownload", QString("%1 threw an exception in acceptable(): %2")
							  .arg(QString::fromStdString(en.name)).arg(e.what()));
		}
	}
	else if (en.regexp.exactMatch(uri))
		return 3;

	return 0;
}

void JavaDownload::startDownload(QString url, QList<QNetworkCookie> cookies, QString referrer)
{
	m_downloadUrl = url;
	CurlDownload::init(url, m_strTarget);
	
	QString clsName = m_plugin->getClass().getClassName();
	if (m_engines[clsName].truncate)
	{
		Segment s;

		s.offset = s.bytes = 0;
		s.urlIndex = 0;
		s.client = 0;

		m_segments.clear();
		m_segments << s;
	}

	m_urls[0].cookies = cookies;
	m_urls[0].strReferrer = referrer;
	CurlDownload::changeActive(true);
}

void JavaDownload::deriveName()
{
	QString name;
	name = QFileInfo(m_strOriginal).fileName();
	name = QUrl::fromPercentEncoding(name.toUtf8());
	m_strName = (!name.isEmpty() && name != "/" && name != ".") ? name : "default.html";
}

void JavaDownload::globalInit(bool forceJreSearch)
{
	new JVM(forceJreSearch);

	if (!JVM::JVMAvailable())
		return;

	// search for plugins
	try
	{
		JClass helper("info.dolezel.fatrat.plugins.helpers.NativeHelpers");
		JClass annotation("info.dolezel.fatrat.plugins.annotations.DownloadPluginInfo");
		QList<QVariant> args;

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
				QString clsName = obj.getClassName();
				JObject instance(obj, JSignature());

				qDebug() << "Class name:" << clsName;
				qDebug() << "Name:" << name;
				qDebug() << "Regexp:" << regexp;

				JavaEngine e = { name.toStdString(), clsName.toStdString(), QRegExp(regexp) };
				e.forceSingleTransfer = ann.call("forceSingleTransfer", JSignature().retBoolean()).toBool();
				e.truncate = ann.call("truncIncomplete", JSignature().retBoolean()).toBool();

				if (instance.instanceOf("info.dolezel.fatrat.plugins.extra.URLAcceptableFilter"))
					e.ownAcceptable = instance;
				m_engines[clsName] = e;

				qDebug() << "createInstance of " << clsName;

				EngineEntry entry;
				entry.longName = m_engines[clsName].name.c_str();
				entry.shortName = m_engines[clsName].shortName.c_str();
				entry.lpfnAcceptable2 = JavaDownload::acceptable;
				entry.lpfnCreate2 = JavaDownload::createInstance;
				entry.lpfnInit = 0;
				entry.lpfnExit = 0;
				entry.lpfnMultiOptions = 0;

				addTransferClass(entry, Transfer::Download);
			}
			catch (const JException& e)
			{
				qDebug() << e.what();
			}
		}
	}
	catch (const RuntimeException& e)
	{
		qDebug() << e.what();
	}

	SettingsItem si;
	si.title = tr("Extensions");
	si.icon = DelayedIcon(":/menu/java_plugin.png");
	si.lpfnCreate = SettingsJavaPluginForm::create;
	addSettingsPage(si);
}

void JavaDownload::globalExit()
{
	qDeleteAll(m_mutexes);
	delete JVM::instance();
}

void JavaDownload::setMessage(QString msg)
{
	m_strMessage = msg;
}

WidgetHostChild* JavaDownload::createOptionsWidget(QWidget* w)
{
	return new JavaDownloadOptsForm(w, this);
}

JavaDownloadOptsForm::JavaDownloadOptsForm(QWidget* me, JavaDownload* myobj)
	: m_download(myobj)
{
	setupUi(me);
}

void JavaDownloadOptsForm::load()
{
	labelClass->setText(m_download->m_strClass);
	lineURL->setText(m_download->m_strOriginal);
}

void JavaDownloadOptsForm::accepted()
{
	QString newUrl = lineURL->text();

	if (newUrl != m_download->m_strOriginal)
	{
		m_download->m_strOriginal = newUrl;
		if (m_download->isActive())
			m_download->setState(Transfer::Waiting); // restart
	}
}

bool JavaDownloadOptsForm::accept()
{
	QString newUrl = lineURL->text();

	if (newUrl != m_download->m_strOriginal)
	{
		if (!JavaDownload::m_engines[m_download->m_strClass].regexp.exactMatch(newUrl))
		{
			QMessageBox::warning(getMainWindow(), "FatRat", tr("Invalid URL."));
			return false;
		}
	}
	return true;
}
