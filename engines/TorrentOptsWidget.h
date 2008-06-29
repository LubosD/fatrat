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

#ifndef TORRENTOPTSWIDGET_H
#define TORRENTOPTSWIDGET_H
#include <QObject>
#include <QTimer>
#include "ui_TorrentOptsWidget.h"
#include "WidgetHostChild.h"
#include "TorrentDownload.h"

class TorrentOptsWidget : public QObject, public WidgetHostChild, Ui_TorrentOptsWidget
{
Q_OBJECT
public:
	TorrentOptsWidget(QWidget* me, TorrentDownload* parent);
	
	virtual void load();
	virtual void accepted();
	void startInvalid();
	
	static void recursiveCheck(QTreeWidgetItem* item, Qt::CheckState state);
	static void recursiveUpdate(QTreeWidgetItem* item);
	static qint64 recursiveUpdateDown(QTreeWidgetItem* item);
public slots:
	void addUrlSeed();
	void addTracker();
	void removeTracker();
	void handleInvalid();
	
	void fileItemChanged(QTreeWidgetItem* item, int column);
private:
	TorrentDownload* m_download;
	QList<QTreeWidgetItem*> m_files;
	QStringList m_seeds;
	std::vector<libtorrent::announce_entry> m_trackers;
	bool m_bUpdating;
	QTimer m_timer;
};

#endif
