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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
USA.
*/

#ifndef FTPUPLOAD_H
#define FTPUPLOAD_H

#include "config.h"

#include "Transfer.h"
#include "FtpClient.h"
#include <QUuid>
#include "ui_FtpUploadOptsForm.h"
#include "WidgetHostChild.h"

#ifdef WITH_SFTP
#	define FTPUPLOAD_DESCR "FTP/SFTP upload"
#else
#	define FTPUPLOAD_DESCR "FTP upload"
#endif

class FtpUpload : public Transfer
{
Q_OBJECT
public:
	FtpUpload();
	virtual ~FtpUpload();
	
	static Transfer* createInstance() { return new FtpUpload; }
	static int acceptable(QString url, bool bDrop);
	
	virtual void init(QString source, QString target);
	virtual void setObject(QString source);
	
	virtual void changeActive(bool nowActive);
	virtual void setSpeedLimits(int, int up);
	
	virtual QString object() const { return m_strSource; }
	virtual QString myClass() const { return "FtpUpload"; }
	virtual QString name() const { return m_strName; }
	virtual QString message() const { return m_strMessage; }
	virtual Mode primaryMode() const { return Upload; }
	virtual void speeds(int& down, int& up) const;
	virtual qulonglong total() const { return m_nTotal; }
	virtual qulonglong done() const;
	
	virtual void load(const QDomNode& map);
	virtual void save(QDomDocument& doc, QDomNode& map) const;
	virtual WidgetHostChild* createOptionsWidget(QWidget*);
	
	virtual void fillContextMenu(QMenu& menu);
private:
	void safeDestroy();
private slots:
	void finished(bool error);
	void status(QString);
	void computeHash();
protected:
	QString m_strName, m_strSource;
	QUrl m_strTarget;
	QString m_strMessage, m_strBindAddress;
	LimitedSocket* m_engine;
	int m_nUpLimit;
	qint64 m_nTotal;
	mutable qint64 m_nDone;
	FtpMode m_mode;
	QUuid m_proxy;
	
	friend class FtpUploadOptsForm;
};

class FtpUploadOptsForm : public QObject, public WidgetHostChild, Ui_FtpUploadOptsForm
{
Q_OBJECT
public:
	FtpUploadOptsForm(QWidget* me,FtpUpload* myobj);
	virtual void load();
	virtual void accepted();
	virtual bool accept();
private:
	FtpUpload* m_upload;
};

#endif
