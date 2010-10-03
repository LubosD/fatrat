/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2010 Lubos Dolezel <lubos a dolezel.info>

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

#ifndef JAVADOWNLOAD_H
#define JAVADOWNLOAD_H
#include "config.h"

#ifndef WITH_JPLUGINS
#	error This file is not supposed to be included!
#endif

#include "CurlDownload.h"
#include <QTimer>
#include <QMap>
#include <QMutex>

class QNetworkReply;
class QNetworkAccessManager;

class JavaDownload : public CurlDownload
{
Q_OBJECT
public:
	JavaDownload(const char* cls);
	virtual ~JavaDownload();

	static void globalInit();
	static void globalExit();

	virtual void init(QString source, QString target);
	virtual QString myClass() const;
	virtual QString name() const;

	virtual void load(const QDomNode& map);
	virtual void save(QDomDocument& doc, QDomNode& map) const;
	virtual void changeActive(bool bActive);
	virtual QObject* createDetailsWidget(QWidget* w) { return 0; }

	virtual void fillContextMenu(QMenu& menu) {}

	virtual QString object() const { return m_strTarget; }
	virtual void setObject(QString newdir);

	virtual qulonglong done() const;
	virtual void setState(State newState);
	virtual QString remoteURI() const;

	static Transfer* createInstance(EngineEntry* e) { return new JavaDownload(e->shortName); }
	static int acceptable(QString uri, bool);
protected slots:
	void httpFinished(QNetworkReply* reply);
	void secondElapsed();
protected:
	void deriveName();
private:
	QString m_strClass;
	QString m_strOriginal, m_strName, m_strTarget;
	QUrl m_downloadUrl;
	QNetworkAccessManager* m_network;
	int m_nSecondsLeft;
	QTimer m_timer;
	QUuid m_proxy;
	bool m_bHasLock;

	static QMap<QString,QMutex*> m_mutexes;
};

#endif // JAVADOWNLOAD_H
