/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation
with the OpenSSL special exemption.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef CREATETORRENTDLG_H
#define CREATETORRENTDLG_H
#include <QDialog>
#include <QList>
#include <QPair>
#include <QThread>
#include "ui_CreateTorrentDlg.h"
#include <libtorrent/create_torrent.hpp>

class HasherThread;
class CreateTorrentDlg : public QDialog, Ui_CreateTorrentDlg
{
Q_OBJECT
public:
	CreateTorrentDlg(QWidget* parent);
	static QWidget* create();
private:
	static void recurseDir(QList<QPair<QString, qint64> >& list, QString prefix, QString path);
public slots:
	void browseFiles();
	void browseDirs();
	void createTorrent();
	void hasherFinished();
private:
	HasherThread* m_hasher;
	QPushButton* pushCreate;
};

class HasherThread : public QThread
{
Q_OBJECT
public:
	HasherThread(QByteArray baseDir, libtorrent::create_torrent* info, QObject* parent);
	~HasherThread();
	
	virtual void run();
	const QString& error() const { return m_strError; }
	libtorrent::create_torrent* info() { return m_info; }
signals:
	void progress(int pos);
private:
	QByteArray m_baseDir;
	libtorrent::create_torrent* m_info;
	bool m_bAbort;
	QString m_strError;
};

#endif
