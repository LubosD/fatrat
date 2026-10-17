/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2026 Lubos Dolezel <lubos a dolezel.info>

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

// Python headers before Qt; see src/python/PyRef.h.
#include "python/PyPython.h"

#include "engines/PythonUpload.h"

#include <QFileInfo>
#include <QtDebug>

#include "CurlPoller.h"
#include "Logger.h"
#include "Settings.h"
#include "fatrat.h"
#include "python/PyPlugin.h"

namespace py = pybind11;

QMap<QString, PythonUpload::PyEngine>& PythonUpload::engines()
{
	static QMap<QString, PyEngine> engines;
	return engines;
}

PythonUpload::PythonUpload(const char* identifier)
	: m_lease(QLatin1String(identifier)), m_strIdentifier(identifier), m_nTotal(0), m_nDone(0), m_nThisPart(0), m_nPartEnd(-1),
	  m_plugin(0), m_curl(0), m_mime(0)
{
	m_errorBuffer[0] = 0;

	if (!engines().contains(m_strIdentifier))
	{
		Logger::global()->enterLogMessage(QLatin1String("Python"),
			tr("No such plugin: %1").arg(m_strIdentifier));
		return;
	}

	const PyEngine& engine = engines()[m_strIdentifier];

	py::gil_scoped_acquire gil;

	try
	{
		py::object instance = pyFromRef(engine.cls)();

		m_pluginObject = pyToRef(instance);
		m_plugin = instance.cast<PyPlugin*>();
		m_plugin->setPluginName(QString::fromStdString(engine.name));
		m_plugin->setTransfer(this);
	}
	catch (const py::error_already_set& e)
	{
		Logger::global()->enterLogMessage(QLatin1String("Python"),
			tr("Failed to create %1: %2").arg(m_strIdentifier).arg(pyErrorText(e)));
	}
}

PythonUpload::~PythonUpload()
{
	if (isActive())
		changeActive(false);

	py::gil_scoped_acquire gil;

	m_task.reset();
	m_uploadFuture.reset();
	m_plugin = 0;
	m_pluginObject.reset();

	if (m_mime)
	{
		curl_mime_free(m_mime);
		m_mime = 0;
	}

	if (m_curl)
	{
		curl_easy_cleanup(m_curl);
		m_curl = 0;
	}
}

void PythonUpload::init(QString source, QString)
{
	setObject(source);
}

void PythonUpload::setObject(QString source)
{
	m_strSource = source;
	m_nTotal = QFileInfo(source).size();

	int pos = m_strSource.lastIndexOf('/');
	m_strName = (pos < 0) ? m_strSource : m_strSource.mid(pos + 1);
}

void PythonUpload::failWith(const QString& message)
{
	setMessage(message);
	enterLogMessage(message);
	setState(Failed);
}

void PythonUpload::changeActive(bool nowActive)
{
	m_buffer.clear();

	if (nowActive)
	{
		m_file.setFileName(m_strSource);

		if (!m_file.open(QIODevice::ReadOnly))
		{
			failWith(m_file.errorString());
			return;
		}

		if (!m_plugin)
		{
			failWith(tr("The plugin %1 could not be loaded").arg(m_strIdentifier));
			return;
		}

		py::gil_scoped_acquire gil;

		try
		{
			py::object coroutine = pyFromRef(m_pluginObject).attr("process_file")(m_strSource.toStdString());

			m_task = pyToRef(pySpawnTask(coroutine, [this](py::object, py::object error)
			{
				m_task.reset();

				if (error.is_none())
					return;

				py::object cancelled = pyModuleAsync().attr("CancelledError");
				if (py::isinstance(error, cancelled))
					return;

				failWith(QString::fromStdString(py::str(error).cast<std::string>()));
			}));
		}
		catch (const py::error_already_set& e)
		{
			failWith(pyErrorText(e));
		}

		return;
	}

	CurlPoller::instance()->removeTransfer(this, true);
	resetStatistics();

	m_curl = 0;

	if (m_mime)
	{
		curl_mime_free(m_mime);
		m_mime = 0;
	}

	py::gil_scoped_acquire gil;

	if (!m_task.isNull())
	{
		py::object task = pyFromRef(m_task);
		m_task.reset();
		task.attr("cancel")();
	}

	m_uploadFuture.reset();

	if (m_plugin)
		m_plugin->abort();
}

void PythonUpload::setSpeedLimits(int, int up)
{
	m_up.max = up;
}

void PythonUpload::speeds(int& down, int& up) const
{
	down = up = 0;

	if (m_curl)
		CurlUser::speeds(down, up);
}

qulonglong PythonUpload::done() const
{
	if (state() == Completed)
		return m_nTotal;
	if (!isActive() || !m_file.isOpen())
		return m_nDone;
	return m_file.pos();
}

void PythonUpload::load(const QDomNode& map)
{
	setObject(getXMLProperty(map, "pyplugin_source"));
	m_nDone = getXMLProperty(map, "pyplugin_done").toLongLong();

	loadVars(map);

	Transfer::load(map);
}

void PythonUpload::save(QDomDocument& doc, QDomNode& map) const
{
	Transfer::save(doc, map);

	saveVars(doc, map);

	setXMLProperty(doc, map, "pyplugin_source", m_strSource);
	setXMLProperty(doc, map, "pyplugin_done", QString::number(m_nDone));
}

CURL* PythonUpload::curlHandle()
{
	return m_curl;
}

void PythonUpload::curlInit()
{
	if (m_curl)
		curl_easy_cleanup(m_curl);
	if (m_mime)
	{
		curl_mime_free(m_mime);
		m_mime = 0;
	}

	m_curl = curl_easy_init();

	if (getSettingsValue("httpftp/forbidipv6").toInt() != 0)
		curl_easy_setopt(m_curl, CURLOPT_IPRESOLVE, long(CURL_IPRESOLVE_V4));

	curl_easy_setopt(m_curl, CURLOPT_USERAGENT, "FatRat/" VERSION);
	curl_easy_setopt(m_curl, CURLOPT_ERRORBUFFER, m_errorBuffer);
	curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT, 10L);
	curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, CurlUser::write_function);
	curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, static_cast<CurlUser*>(this));
	curl_easy_setopt(m_curl, CURLOPT_HEADERFUNCTION, process_header);
	curl_easy_setopt(m_curl, CURLOPT_HEADERDATA, this);
}

size_t PythonUpload::process_header(const char* ptr, size_t size, size_t nmemb, PythonUpload* This)
{
	QByteArray line = QByteArray(ptr, size * nmemb).trimmed();
	int pos = line.indexOf(": ");

	if (pos != -1)
		This->m_headers[QString::fromLatin1(line.left(pos)).toLower()] = QString::fromUtf8(line.mid(pos + 2));

	return size * nmemb;
}

// The file part's bytes. CurlUser::read_function already has curl's callback
// signature and does the upload-speed accounting, so it is reused verbatim.
//
// Both callbacks are handed a CurlUser*, never a PythonUpload*: this class
// inherits CurlUser as its second base, so the two pointers differ, and
// passing the wrong one lands curl in the wrong object.
static int uploadSeek(void* arg, curl_off_t offset, int origin)
{
	Q_UNUSED(arg);
	Q_UNUSED(offset);
	Q_UNUSED(origin);

	// Rewinding mid-upload would need the plugin's form rebuilt; let curl
	// fail the transfer instead of silently sending the wrong bytes.
	return CURL_SEEKFUNC_CANTSEEK;
}

PyRef PythonUpload::startUpload(const QString& url, const QList<MimePart>& parts, qint64 offset, qint64 bytes)
{
	curlInit();

	m_strUrl = url;
	m_buffer.clear();
	m_headers.clear();

	m_nThisPart = (bytes >= 0 ? bytes : m_nTotal) - offset;
	m_nPartEnd = offset + m_nThisPart;
	m_fileName = m_strName.toUtf8();

	if (!m_file.seek(offset))
	{
		py::object future = pyMakeFuture();

		pyFailFuture(future, "PluginError", m_file.errorString());
		return pyToRef(future);
	}

	m_mime = curl_mime_init(m_curl);

	foreach (const MimePart& part, parts)
	{
		curl_mimepart* mimePart = curl_mime_addpart(m_mime);
		QByteArray name = part.name.toUtf8();

		curl_mime_name(mimePart, name.constData());

		if (!part.filePart)
		{
			QByteArray value = part.value.toUtf8();
			curl_mime_data(mimePart, value.constData(), value.size());
		}
		else
		{
			curl_mime_filename(mimePart, m_fileName.constData());
			curl_mime_data_cb(mimePart, curl_off_t(m_nThisPart),
					  (curl_read_callback) CurlUser::read_function,
					  uploadSeek, 0, static_cast<CurlUser*>(this));
		}
	}

	QByteArray urlData = url.toUtf8();

	curl_easy_setopt(m_curl, CURLOPT_URL, urlData.constData());
	curl_easy_setopt(m_curl, CURLOPT_MIMEPOST, m_mime);

	enterLogMessage(tr("Uploading to %1").arg(url));

	py::object future = pyMakeFuture();
	m_uploadFuture = pyToRef(future);

	CurlPoller::instance()->addTransfer(this);

	return pyToRef(future);
}

size_t PythonUpload::readData(char* buffer, size_t maxData)
{
	// Never read past the end of this chunk: the rest belongs to the next
	// upload() the plugin makes.
	qint64 left = m_nPartEnd - m_file.pos();

	if (left <= 0)
		return 0;
	if (qint64(maxData) > left)
		maxData = size_t(left);

	qint64 read = m_file.read(buffer, maxData);

	if (read < 0)
	{
		setMessage(m_file.errorString());
		return 0;
	}

	return size_t(read);
}

bool PythonUpload::writeData(const char* buffer, size_t bytes)
{
	m_buffer.append(buffer, bytes);
	return true;
}

void PythonUpload::transferDone(CURLcode result)
{
	if (result != CURLE_OK)
	{
		QString error = QString::fromLatin1(m_errorBuffer[0] ? m_errorBuffer : curl_easy_strerror(result));

		if (isActive())
			failWith(error);

		py::gil_scoped_acquire gil;

		PyRef future = m_uploadFuture;
		m_uploadFuture.reset();

		if (!future.isNull())
			pyFailFuture(pyFromRef(future), "NetworkError", error);

		return;
	}

	m_nDone += m_nThisPart;
	m_nThisPart = 0;

	// Out of curl's callback before the plugin coroutine resumes.
	QMetaObject::invokeMethod(this, "deliverResponse", Qt::QueuedConnection);
}

void PythonUpload::deliverResponse()
{
	py::gil_scoped_acquire gil;

	PyRef future = m_uploadFuture;
	m_uploadFuture.reset();

	if (future.isNull())
		return;

	try
	{
		py::dict headers;

		for (QMap<QString, QString>::const_iterator it = m_headers.begin(); it != m_headers.end(); it++)
			headers[py::str(it.key().toStdString())] = py::str(it.value().toStdString());

		long status = 0;
		curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &status);

		py::object response = pyModuleHttp().attr("Response")(
			py::arg("url") = m_strUrl.toStdString(),
			py::arg("status") = int(status),
			py::arg("content") = py::bytes(m_buffer.constData(), m_buffer.size()),
			py::arg("headers") = headers);

		pyResolveFuture(pyFromRef(future), response);
	}
	catch (const py::error_already_set& e)
	{
		enterLogMessage(pyErrorText(e));
	}
}

void PythonUpload::putDownloadLink(const QString& downloadLink, const QString& killLink)
{
	enterLogMessage(tr("Download link: %1").arg(downloadLink));

	if (!killLink.isEmpty())
		enterLogMessage(tr("Kill link: %1").arg(killLink));

	setMessage(downloadLink);
	setState(Completed);
}

int PythonUpload::acceptable(QString, bool, const EngineEntry*)
{
	// Uploads are chosen by the user from the engine list, not matched
	// against a URL -- same as the Java upload plugins.
	return 1;
}
