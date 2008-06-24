/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

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
*/

#ifndef CURLDOWNLOAD_H
#define CURLDOWNLOAD_H
#include "Transfer.h"
#include "fatrat.h"
#include "CurlUser.h"
#include <QHash>
#include <QUuid>
#include <QFile>
#include <QDir>
#include <QUrl>

class CurlDownload : public Transfer, public CurlUser
{
Q_OBJECT
public:
	CurlDownload();
	~CurlDownload();
	
	virtual void init(QString source, QString target);
	virtual void changeActive(bool bActove);
	virtual void setObject(QString object);
	virtual QString object() const;
	virtual QString myClass() const { return "CurlDownload"; }
	virtual QString message() const { return m_strMessage; }
	virtual QString name() const;
	virtual void speeds(int& down, int& up) const;
	virtual qulonglong total() const;
	virtual qulonglong done() const;
	virtual void load(const QDomNode& map);
	virtual void save(QDomDocument& doc, QDomNode& map) const;
	virtual void setSpeedLimits(int down, int up);
	
	static int acceptable(QString uri, bool);
	static Transfer* createInstance() { return new CurlDownload; }
	static void globalInit();
	static void globalExit();
protected:
	virtual CURL* curlHandle();
	virtual void speedLimits(int& down, int& up);
	virtual bool writeData(const char* buffer, size_t bytes);
	virtual void transferDone(CURLcode result);
private slots:
	void switchMirror();
	void computeHash();
private:
	void generateName();
	void init2(QString uri, QString dest);
	void setTargetName(QString newFileName);
	void fillContextMenu(QMenu& menu);
	QString filePath() const;
	void processHeaders();
	
	static size_t process_header(const char* ptr, size_t size, size_t nmemb, CurlDownload* This);
	static int curl_debug_callback(CURL*, curl_infotype, char* text, size_t bytes, CurlDownload* This);
private:
	CURL* m_curl;
	QDir m_dir;
	qulonglong m_nTotal;
	mutable qulonglong m_nStart;
	
	QString m_strFile, m_strMessage;
	QFile m_file;
	bool m_bAutoName;
	QHash<QByteArray, QByteArray> m_headers;
	
	char m_errorBuffer[CURL_ERROR_SIZE];
	
	struct UrlObject
	{
		QUrl url;
		QString strReferrer, strBindAddress;
		FtpMode ftpMode;
		QUuid proxy;
	};
	QList<UrlObject> m_urls;
	int m_nUrl;
	
	friend class HttpOptsWidget;
	friend class HttpUrlOptsDlg;
};

#endif
