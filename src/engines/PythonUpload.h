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

#ifndef PYTHONUPLOAD_H
#define PYTHONUPLOAD_H

#include "config.h"
#ifndef WITH_PYPLUGINS
#	error This file is not supposed to be included!
#endif

#include <QByteArray>
#include <QFile>
#include <QList>
#include <QMap>
#include <QString>
#include <string>

#include "CurlUser.h"
#include "PythonPersistentVariables.h"
#include "StaticTransferMessage.h"
#include "Transfer.h"
#include "python/PyRef.h"

class PyPlugin;

/* An upload driven by a Python plugin.
 *
 * The plugin decides what the form looks like and what to do with the
 * server's answer; FatRat does the HTTP, through the same curl machinery
 * (and the same speed accounting and limits) as every other transfer.
 *
 * A plugin awaits each upload() and inspects the response, so multi-step
 * protocols -- ask for an upload server, upload, then confirm -- read as
 * straight-line code.
 */
class PythonUpload : public StaticTransferMessage<Transfer>, public CurlUser, public PythonPersistentVariables
{
Q_OBJECT
public:
	PythonUpload(const char* identifier);
	virtual ~PythonUpload();

	static Transfer* createInstance(const EngineEntry* e) { return new PythonUpload(e->shortName); }
	static int acceptable(QString uri, bool, const EngineEntry* e);

	virtual void init(QString source, QString target);
	virtual void setObject(QString source);

	virtual void changeActive(bool nowActive);
	virtual void setSpeedLimits(int, int up);

	virtual QString object() const { return m_strSource; }
	virtual QString myClass() const { return m_strIdentifier; }
	virtual QString name() const { return m_strName; }
	virtual QString message() const { return m_strMessage; }
	virtual Mode primaryMode() const { return Upload; }
	virtual void speeds(int& down, int& up) const;
	virtual qulonglong total() const { return m_nTotal; }
	virtual qulonglong done() const;

	virtual void load(const QDomNode& map);
	virtual void save(QDomDocument& doc, QDomNode& map) const;

	struct MimePart
	{
		MimePart() : filePart(false) {}

		QString name, value;
		bool filePart;
	};

	struct PyEngine
	{
		std::string name, identifier;
		PyRef cls;
	};

	static QMap<QString, PyEngine>& engines();

	// Called by the plugin: POST one multipart form, optionally sending only
	// part of the file. Resolves with the server's response.
	PyRef startUpload(const QString& url, const QList<MimePart>& parts, qint64 offset, qint64 bytes);
	void putDownloadLink(const QString& downloadLink, const QString& killLink);
protected:
	void curlInit();
	void failWith(const QString& message);

	virtual CURL* curlHandle();
	virtual void transferDone(CURLcode result);
	virtual size_t readData(char* buffer, size_t maxData);
	virtual bool writeData(const char* buffer, size_t bytes);

	static size_t process_header(const char* ptr, size_t size, size_t nmemb, PythonUpload* This);
private slots:
	void deliverResponse();
private:
	QString m_strIdentifier, m_strSource, m_strName, m_strUrl;
	qint64 m_nTotal, m_nDone, m_nThisPart, m_nPartEnd;

	PyPlugin* m_plugin;
	PyRef m_pluginObject, m_task, m_uploadFuture;

	CURL* m_curl;
	curl_mime* m_mime;
	QFile m_file;
	QByteArray m_buffer;
	QMap<QString, QString> m_headers;
	QByteArray m_fileName;
	char m_errorBuffer[CURL_ERROR_SIZE];
};

#endif // PYTHONUPLOAD_H
