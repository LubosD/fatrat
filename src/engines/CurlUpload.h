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

#ifndef CURLUPLOAD_H
#define CURLUPLOAD_H
#include "Transfer.h"
#include "CurlUser.h"
#include "fatrat.h"
#include <QUuid>
#include <QFile>
#include <QUrl>
#include <curl/curl.h>

class CurlUpload : public Transfer, public CurlUser
{
Q_OBJECT
public:
	CurlUpload();
	virtual ~CurlUpload();
	
	static Transfer* createInstance() { return new CurlUpload; }
	static int acceptable(QString url, bool bDrop);
	
	virtual void init(QString source, QString target);
	virtual void setObject(QString source);
	
	virtual void changeActive(bool nowActive);
	virtual void setSpeedLimits(int, int up);
	
	virtual QString object() const { return m_strSource; }
	virtual QString myClass() const { return "CurlUpload"; }
	virtual QString name() const { return m_strName; }
	virtual QString message() const { return m_strMessage; }
	virtual Mode primaryMode() const { return Upload; }
	virtual void speeds(int& down, int& up) const;
	virtual qulonglong total() const { return m_nTotal; }
	virtual qulonglong done() const;
	
	virtual void load(const QDomNode& map);
	virtual void save(QDomDocument& doc, QDomNode& map) const;
	//virtual WidgetHostChild* createOptionsWidget(QWidget*);
	
	//virtual void fillContextMenu(QMenu& menu);
protected:
	virtual CURL* curlHandle();
	virtual void transferDone(CURLcode result);
	virtual size_t readData(char* buffer, size_t maxData);
	
	static int seek_function(CurlUpload* This, curl_off_t offset, int origin);
	static int curl_debug_callback(CURL*, curl_infotype type, char* text, size_t bytes, CurlUpload* This);
private:
	CURL* m_curl;
	qint64 m_nDone, m_nTotal;
	QFile m_file;
	QString m_strSource, m_strMessage, m_strName;
	QUrl m_strTarget;
	FtpMode m_mode;
	QUuid m_proxy;
	char m_errorBuffer[CURL_ERROR_SIZE];
};

#endif
