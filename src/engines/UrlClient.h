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
#include <curl/curl.h>
#include "engines/CurlUser.h"

class UrlClient : public QObject, public CurlUser
{
Q_OBJECT
public:
	UrlClient();
	~UrlClient();
	
	enum FtpMode { FtpActive = 0, FtpPassive };
	struct UrlObject
	{
		QUrl url;
		QString strReferrer, strBindAddress;
		FtpMode ftpMode;
		QUuid proxy;
	};
	
	void start();
	void stop();
	
	void setSourceObject(const UrlObject& obj);
	void setTargetObject(QIODevice* dev);
	void setRange(qlonglong from, qlonglong to);
	qlonglong progress() const;
	
	virtual CURL* curlHandle();
	virtual bool writeData(const char* buffer, size_t bytes);
	virtual void transferDone(CURLcode result);
protected:
	static size_t process_header(const char* ptr, size_t size, size_t nmemb, UrlClient* This);
	static int curl_debug_callback(CURL*, curl_infotype, char* text, size_t bytes, UrlClient* This);
	void processHeaders();
signals:
	void renameTo(QString name);
	void logMessage(QString msg);
	void done(QString error);
	void totalSizeKnown(qlonglong bytes);
private:
	UrlObject m_source;
	QIODevice* m_target;
	qlonglong m_rangeFrom, m_rangeTo, m_progress;
	CURL* m_curl;
	char m_errorBuffer[CURL_ERROR_SIZE];
	QHash<QByteArray, QByteArray> m_headers;
};

#endif
