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
#include "engines/CurlUser.h"
#include "engines/UrlClient.h"
#include "engines/CurlPollingMaster.h"
#include <QReadWriteLock>
#include <QTimer>
#include <QColor>
#include <QHash>
#include <QUuid>
#include <QFile>
#include <QDir>
#include <QUrl>

class CurlDownload : public Transfer
{
Q_OBJECT
public:
	CurlDownload();
	~CurlDownload();
	
	virtual void init(QString source, QString target);
	virtual void changeActive(bool bActove);
	virtual void setObject(QString object);
	virtual QString object() const;
	virtual QString myClass() const { return "GeneralDownload"; }
	virtual QString message() const { return m_strMessage; }
	virtual QString name() const;
	virtual void speeds(int& down, int& up) const;
	virtual qulonglong total() const;
	virtual qulonglong done() const;
	virtual void load(const QDomNode& map);
	virtual void save(QDomDocument& doc, QDomNode& map) const;
	virtual void setSpeedLimits(int down, int up);
	
	static int acceptable(QString uri, bool);
	static QDialog* createMultipleOptionsWidget(QWidget* parent, QList<Transfer*>& transfers);
	static Transfer* createInstance() { return new CurlDownload; }
	static void globalInit();
	static void globalExit();
	
	virtual WidgetHostChild* createOptionsWidget(QWidget* w);
	virtual QObject* createDetailsWidget(QWidget* w);
	virtual void fillContextMenu(QMenu& menu);
protected:
	QString filePath() const;
private slots:
	void computeHash();
	void clientRenameTo(QString name);
	void clientLogMessage(QString msg);
	void clientDone(QString error);
	void clientTotalSizeKnown(qlonglong bytes);
	void updateSegmentProgress();
protected:
	struct Segment
	{
		// the start
		qlonglong offset;
		// how many bytes have already been transfered
		qlonglong bytes;
		// the last url object used for this segment
		int urlIndex;
		// pointer to a UrlClient instance, if the segment is active
		UrlClient* client;
		QColor color;
	};
	
	void generateName();
	void init2(QString uri, QString dest);
	void setTargetName(QString newFileName);
	void processHeaders();
	void autoCreateSegment();
	void removeLostSegments();
	static void simplifySegments(QList<Segment>& in);
	QColor allocateSegmentColor();
protected:
	QDir m_dir;
	qulonglong m_nTotal;
	
	QString m_strFile, m_strMessage;
	bool m_bAutoName;
	QHash<QByteArray, QByteArray> m_headers;
	
	char m_errorBuffer[CURL_ERROR_SIZE];
	
	QList<UrlClient::UrlObject> m_urls;
	QList<Segment> m_segments;
	mutable QReadWriteLock m_segmentsLock;
	CurlPollingMaster* m_master;
	UrlClient* m_nameChanger;
	QTimer m_timer;
	
	friend class HttpOptsWidget;
	friend class HttpUrlOptsDlg;
	friend class HttpDetailsBar;
};

#endif
