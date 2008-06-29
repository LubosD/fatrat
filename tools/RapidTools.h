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

#ifndef RAPIDTOOLS_H
#define RAPIDTOOLS_H
#include <QWidget>
#include <QHttp>
#include <QBuffer>
#include <QMap>
#include "ui_RapidTools.h"

class RapidTools : public QWidget, Ui_RapidTools
{
Q_OBJECT
public:
	RapidTools();
	static QWidget* create() { return new RapidTools; }
public slots:
	void checkRShareLinks();
	void downloadRShareLinks();
	void doneRShare(bool error);
	void reserRShare();
	
	void decodeRSafeLinks();
	void downloadRSafeLinks();
	void doneRSafe(int r, bool error);
	void doneRSafe(bool);
	
	void extractRFLinks();
	void downloadRFLinks();
	void doneRF(bool error);
private:
	void doRShareCheck();
	
	QHttp *m_httpRShare, *m_httpRSafe, *m_httpRF;
	QBuffer *m_bufRShare, *m_bufRF;
	
	QMap<unsigned long, QString> m_mapRShare;
	QString m_strRShareWorking;
	QStringList m_listRSharePending;
	
	QMap<int, QString> m_listRSafeSrc;
	QMap<int, QBuffer*> m_mapRSafeBufs;
};

#endif
