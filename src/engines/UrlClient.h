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

#ifndef URLCLIENT_H
#define URLCLIENT_H
#include <QUrl>
#include <QString>
#include <QUuid>
#include <QHash>
#include <QByteArray>
#include <QNetworkCookie>
#include <curl/curl.h>
#include "engines/CurlUser.h"

class CurlPollingMaster;

class UrlClient : public QObject, public CurlUser
{
Q_OBJECT
public:
	UrlClient();
	~UrlClient();
	
	enum FtpMode { FtpActive = 0, FtpPassive };
	struct UrlObject
	{
		QUrl url, effective;
		QString strReferrer, strBindAddress, strUserAgent;
		QByteArray strPostData;
		FtpMode ftpMode;
		QUuid proxy;
		QList<QNetworkCookie> cookies;
	};
	
	void start();
	void stop();
	
	void setSourceObject(UrlObject& obj);
	void setTargetObject(int fd);
	// The range is in form <from, to)
	void setRange(qlonglong from, qlonglong to);
	qlonglong progress() const;
	qlonglong rangeFrom() const { return m_rangeFrom; }
	qlonglong rangeTo() const { return m_rangeTo; }
	void setPollingMaster(CurlPollingMaster* master);
	
	virtual CURL* curlHandle();
	virtual bool writeData(const char* buffer, size_t bytes);
	virtual void transferDone(CURLcode result);
protected:
	static size_t process_header(const char* ptr, size_t size, size_t nmemb, UrlClient* This);
	static int curl_debug_callback(CURL*, curl_infotype, char* text, size_t bytes, UrlClient* This);
	void processHeaders();
signals:
	void failure(QString msg);
	void renameTo(QString name);
	void logMessage(QString msg);
	void done(QString error = QString());
	void totalSizeKnown(qlonglong bytes);
	void rangesUnsupported();
private:
	UrlObject* m_source;
	int m_target;
	qlonglong m_rangeFrom, m_rangeTo, m_progress;
	CURL* m_curl;
	char m_errorBuffer[CURL_ERROR_SIZE];
	char* m_postData;
	QHash<QByteArray, QByteArray> m_headers;
	//CurlPollingMaster* m_master;
	bool m_bTerminating;
};

#endif
