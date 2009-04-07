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

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#ifndef RAPIDSHAREUPLOAD_H
#define RAPIDSHAREUPLOAD_H

#include "Transfer.h"
#include "CurlUser.h"
#include "WidgetHostChild.h"
#include "ui_RapidshareOptsForm.h"
#include <QFile>
#include <QBuffer>

class QHttp;
class RapidshareStatusWidget;

class RapidshareUpload : public Transfer, public CurlUser
{
Q_OBJECT
public:
	RapidshareUpload();
	virtual ~RapidshareUpload();
	
	static void globalInit();
	static void applySettings();
	static Transfer* createInstance() { return new RapidshareUpload; }
	static int acceptable(QString url, bool);
	
	virtual void init(QString source, QString target);
	virtual void setObject(QString source);
	
	virtual void changeActive(bool nowActive);
	virtual void setSpeedLimits(int, int up);
	
	virtual QString object() const { return m_strSource; }
	virtual QString myClass() const { return "RapidshareUpload"; }
	virtual QString name() const { return m_strName; }
	virtual QString message() const { return m_strMessage; }
	virtual Mode primaryMode() const { return Upload; }
	virtual void speeds(int& down, int& up) const;
	virtual qulonglong total() const { return m_nTotal; }
	virtual qulonglong done() const;
	
	virtual void load(const QDomNode& map);
	virtual void save(QDomDocument& doc, QDomNode& map) const;
	virtual WidgetHostChild* createOptionsWidget(QWidget*);
	virtual QObject* createDetailsWidget(QWidget* widget);
	
	static QDialog* createMultipleOptionsWidget(QWidget* parent, QList<Transfer*>& transfers);
	static int curl_debug_callback(CURL*, curl_infotype type, char* text, size_t bytes, RapidshareUpload* This);
	static int seek_function(RapidshareUpload* file, curl_off_t offset, int origin);
	static qint64 chunkSize();
	
	void beginNextChunk();
protected slots:
	void queryDone(bool error);
protected:
	void saveLink(QString file, QString link);
	void curlInit();
	
	virtual CURL* curlHandle();
	virtual void transferDone(CURLcode result);
	virtual size_t readData(char* buffer, size_t maxData);
	virtual bool writeData(const char* buffer, size_t bytes);
protected:
	enum AccountType { AccountNone = 0, AccountCollector, AccountPremium };
	enum QueryType { QueryNone = 0, QueryFileInfo, QueryServerID };
	
	qint64 m_nTotal;
	QString m_strName, m_strSource, m_strMessage, m_strSafeName;
	QString m_strUsername, m_strPassword, m_strServer;
	AccountType m_type;
	QueryType m_query;
	qint64 m_nFileID, m_nDone; // for resume
	QString m_strKillID;
	bool m_bIDJustChecked;
	QUuid m_proxy;
	QString m_strLinksDownload, m_strLinksKill;
	
	CURL* m_curl;
	curl_httppost* m_postData;
	QHttp* m_http;
	QBuffer* m_buffer;
	QFile m_file;
	char m_errorBuffer[CURL_ERROR_SIZE];
	
	static RapidshareStatusWidget* m_labelStatus;
	
	friend class RapidshareOptsWidget;
	friend class RapidshareUploadDetails;
	friend class RSProgressWidget;
};

class RapidshareOptsWidget : public QObject, public WidgetHostChild, Ui_RapidshareOptsForm
{
Q_OBJECT
public:
	RapidshareOptsWidget(QWidget* me, RapidshareUpload* myobj);
	RapidshareOptsWidget(QWidget* me, QList<Transfer*>* multi, QObject* parent);
	virtual void load();
	virtual void accepted();
	virtual bool accept();
	
	static void browse(QLineEdit* target);
protected slots:
	void accTypeChanged(int now);
	void browseDownload();
	void browseKill();
private:
	void init(QWidget* me);
	
	RapidshareUpload* m_upload;
	QList<Transfer*>* m_multi;
};

class RapidshareSettings : public QObject, public WidgetHostChild, Ui_RapidshareOptsForm
{
Q_OBJECT
public:
	RapidshareSettings(QWidget* me, QObject* parent);
	static WidgetHostChild* create(QWidget* me, QObject* parent) { return new RapidshareSettings(me, parent); }
	virtual void load();
	virtual void accepted();
	virtual bool accept();
protected slots:
	void accTypeChanged(int now);
	void browseDownload();
	void browseKill();
};

#endif
