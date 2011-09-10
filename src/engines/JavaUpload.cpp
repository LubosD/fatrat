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

#include "JavaUpload.h"
#include <QFileInfo>
#include <cassert>
#include "RuntimeException.h"
#include "Settings.h"
#include "fatrat.h"
#include "CurlPoller.h"
#include "java/JByteBuffer.h"
#include "java/JVM.h"
#include "java/JArray.h"
#include "java/JException.h"
#include <QtDebug>

QMap<QString,JavaUpload::JavaEngine> JavaUpload::m_engines;
extern QList<JObject> g_configListeners;

JavaUpload::JavaUpload(const char* cls)
: m_nTotal(0), m_nDone(0), m_nThisPart(0), m_curl(0), m_postData(0)
{
	qDebug() << "JavaUpload::JavaUpload(): " << cls;
	assert(m_engines.contains(cls));

	m_strClass = cls;
	m_plugin = new JUploadPlugin(cls);
	m_plugin->setTransfer(this);
}

JavaUpload::~JavaUpload()
{
	if(isActive())
		changeActive(false);

	delete m_plugin;
	curl_formfree(m_postData);
	m_postData = 0;
}
	
void JavaUpload::globalInit()
{
	if (!JVM::JVMAvailable())
		return;
	
	JUploadPlugin::registerNatives();
	
	// locate Java plugins
	try
	{
		JClass helper("info.dolezel.fatrat.plugins.helpers.NativeHelpers");
		JClass annotation("info.dolezel.fatrat.plugins.annotations.UploadPluginInfo");
		JClass annConfigDialog("info.dolezel.fatrat.plugins.annotations.ConfigDialog");
		QList<QVariant> args;

		args << "info.dolezel.fatrat.plugins" << annotation.toVariant();

		JArray arr = helper.callStatic("findAnnotatedClasses",
						  JSignature().addString().add("java.lang.Class").retA("java.lang.Class"),
						  args).value<JArray>();
		qDebug() << "Found" << arr.size() << "annotated classes (UploadPluginInfo)";

		int classes = arr.size();
		for (int i = 0; i < classes; i++)
		{
			try
			{
				JClass obj = (jobject) arr.getObject(i);
				JObject ann = obj.getAnnotation(annotation);
				QString name = ann.call("name", JSignature().retString()).toString();
				QString clsName = obj.getClassName();
				JObject instance(obj, JSignature());

				qDebug() << "Class name:" << clsName;
				qDebug() << "Name:" << name;

				JObject cfgDlg = obj.getAnnotation(annConfigDialog);

				JavaEngine e = { "EXT - " + name.toStdString(), clsName.toStdString() };

				if (!cfgDlg.isNull())
					e.configDialog = cfgDlg.call("value", JSignature().retString()).toString();

				if (instance.instanceOf("info.dolezel.fatrat.plugins.extra.URLAcceptableFilter"))
					e.ownAcceptable = instance;
				if (instance.instanceOf("info.dolezel.fatrat.plugins.listeners.ConfigListener"))
					g_configListeners << instance;
				m_engines[clsName] = e;

				qDebug() << "createInstance of " << clsName;

				EngineEntry entry;
				entry.longName = m_engines[clsName].name.c_str();
				entry.shortName = m_engines[clsName].shortName.c_str();
				entry.lpfnAcceptable2 = JavaUpload::acceptable;
				entry.lpfnCreate2 = JavaUpload::createInstance;
				entry.lpfnInit = 0;
				entry.lpfnExit = 0;
				entry.lpfnMultiOptions = 0;

				addTransferClass(entry, Transfer::Upload);
			}
			catch (const JException& e)
			{
				qDebug() << e.what();
			}
		}
	}
	catch (const JException& e)
	{
		qDebug() << e.what();
	}
}

void JavaUpload::applySettings()
{
}

int JavaUpload::acceptable(QString uri, bool, const EngineEntry* e)
{
	qDebug() << "JavaUpload::acceptable():" << uri << e->shortName;
	return 1;
}

void JavaUpload::init(QString source, QString target)
{
	setObject(source);
}

void JavaUpload::setObject(QString source)
{
	m_strSource = source;
	m_nTotal = QFileInfo(source).size();
	
	// derive name
	int pos = m_strSource.lastIndexOf('/');
	if (pos < 0)
		m_strName = m_strSource;
	else
		m_strName = m_strSource.mid(pos+1);
}

void JavaUpload::changeActive(bool nowActive)
{
	m_buffer.clear();
	
	if (nowActive)
	{
		m_file.setFileName(m_strSource);
		if (!m_file.open(QIODevice::ReadOnly))
		{
			m_strMessage = m_file.errorString();
			setState(Failed);
			return;
		}
		
		// call Java
		m_plugin->call("processFile", JSignature().addString(), JArgs() << m_strSource);
	}
	else
	{
		CurlPoller::instance()->removeTransfer(this, true);
		resetStatistics();
		
		if(m_curl)
			m_curl = 0;
		
		if(m_postData)
		{
			curl_formfree(m_postData);
			m_postData = 0;
		}
		m_plugin->abort();
	}
}

void JavaUpload::setSpeedLimits(int, int up)
{
	m_up.max = up;
}

void JavaUpload::speeds(int& down, int& up) const
{
	down = up = 0;
	
	if (m_curl)
		CurlUser::speeds(down, up);
}

qulonglong JavaUpload::done() const
{
	if (state() == Completed)
		return m_nTotal;
	if (!isActive() || !m_file.isOpen())
		return m_nDone;
	return m_file.pos();
}

void JavaUpload::load(const QDomNode& map)
{
	setObject(getXMLProperty(map, "jplugin_source"));
	m_nDone = getXMLProperty(map, "jplugin_done").toLongLong();
	loadVars(map);
	
	Transfer::load(map);
}

void JavaUpload::save(QDomDocument& doc, QDomNode& map) const
{
	Transfer::save(doc, map);

	saveVars(doc, map);
	
	setXMLProperty(doc, map, "jplugin_source", m_strSource);
	setXMLProperty(doc, map, "jplugin_done", QString::number(m_nDone));
}

CURL* JavaUpload::curlHandle()
{
	return m_curl;
}

void JavaUpload::curlInit()
{
	if(m_curl)
		curl_easy_cleanup(m_curl);
	
	m_curl = curl_easy_init();
	//curl_easy_setopt(m_curl, CURLOPT_POST, true);
	if(getSettingsValue("httpftp/forbidipv6").toInt() != 0)
		curl_easy_setopt(m_curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
	curl_easy_setopt(m_curl, CURLOPT_USERAGENT, "FatRat/" VERSION);
	curl_easy_setopt(m_curl, CURLOPT_ERRORBUFFER, m_errorBuffer);
	//curl_easy_setopt(m_curl, CURLOPT_SEEKFUNCTION, seek_function);
	curl_easy_setopt(m_curl, CURLOPT_SEEKDATA, this);
	curl_easy_setopt(m_curl, CURLOPT_DEBUGDATA, this);
	curl_easy_setopt(m_curl, CURLOPT_VERBOSE, true);
	//curl_easy_setopt(m_curl, CURLOPT_PROGRESSFUNCTION, anti_crash_fun);
	curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT, 10);
	curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, CurlUser::write_function);
	curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, static_cast<CurlUser*>(this));
	curl_easy_setopt(m_curl, CURLOPT_READFUNCTION, CurlUser::read_function);
	curl_easy_setopt(m_curl, CURLOPT_READDATA, static_cast<CurlUser*>(this));
	curl_easy_setopt(m_curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
	curl_easy_setopt(m_curl, CURLOPT_HEADERFUNCTION, process_header);
	curl_easy_setopt(m_curl, CURLOPT_WRITEHEADER, this);
	
	if(m_postData)
	{
		curl_formfree(m_postData);
		m_postData = 0;
	}
}

size_t JavaUpload::process_header(const char* ptr, size_t size, size_t nmemb, JavaUpload* This)
{
	QByteArray line = QByteArray(ptr, size*nmemb).trimmed();
	int pos = line.indexOf(": ");
	
	if(pos != -1)
		This->m_headers[line.left(pos).toLower()] = line.mid(pos+2);
	
	return size*nmemb;
}


void JavaUpload::transferDone(CURLcode result)
{
	if(result != CURLE_OK)
	{
		if (isActive())
			setState(Failed);
	}
	else
	{
		m_nDone += m_nThisPart;
		m_nThisPart = 0;
		QMetaObject::invokeMethod(this, "checkResponse", Qt::QueuedConnection);
	}
}

void JavaUpload::checkResponse()
{
	// call Java
	JByteBuffer direct(m_buffer.data(), m_buffer.size());
	JMap hdr = JMap::fromQMap(m_headers);
	m_plugin->call("checkResponse", JSignature().add("java.nio.ByteBuffer").add("java.util.Map"), JArgs() << direct << hdr);
}

size_t JavaUpload::readData(char* buffer, size_t maxData)
{
	qint64 read = m_file.read(buffer, maxData);
	if (read < 0)
	{
		m_strMessage = m_file.errorString();
		return 0;
	}
	else
		return size_t( read );
}

bool JavaUpload::writeData(const char* buffer, size_t bytes)
{
	// append to buffer, then pass to Java
	m_buffer.append(buffer, bytes);
	return true;
}

void JavaUpload::putDownloadLink(QString downloadLink, QString killLink)
{
	enterLogMessage(tr("Download link: %1").arg(downloadLink));
	
	if (!killLink.isEmpty())
		enterLogMessage(tr("Kill link: %1").arg(killLink));
	setState(Completed);
}

void JavaUpload::startUpload(QString url, QList<JavaUpload::MimePart>& parts, qint64 offset, qint64 bytes)
{
	curl_httppost* lastData = 0;
	
	curlInit();
	qDebug() << "JavaUpload::startUpload:" << url;
	
	foreach (const MimePart& part, parts)
	{
		QByteArray fieldName = part.name.toUtf8();
		
		if (!part.filePart)
		{
			QByteArray value = part.value.toUtf8();
			curl_formadd(&m_postData, &lastData, CURLFORM_COPYNAME, fieldName.constData(), CURLFORM_COPYCONTENTS, value.constData(), CURLFORM_END);
		}
		else
		{
			QByteArray fileName = m_strName.toUtf8();

			if (bytes >= 0)
				m_nThisPart = bytes;
			else
				m_nThisPart = m_nTotal;
			m_nThisPart -= offset;

			m_file.seek(offset);

			strncpy(m_fileName, fileName.constData(), sizeof(m_fileName-1));
			m_fileName[sizeof(m_fileName-1)] = 0;

			curl_formadd(&m_postData, &lastData, CURLFORM_COPYNAME, fieldName.constData(),
				CURLFORM_STREAM, static_cast<CurlUser*>(this),
				CURLFORM_FILENAME, m_fileName,
				CURLFORM_CONTENTSLENGTH, m_nThisPart, CURLFORM_END);
		}
	}
	
	QByteArray ba = url.toUtf8();
	curl_easy_setopt(m_curl, CURLOPT_URL, ba.constData());
	curl_easy_setopt(m_curl, CURLOPT_HTTPPOST, m_postData);
	
	CurlPoller::instance()->addTransfer(this);
}


