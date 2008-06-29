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

#ifndef RSSDOWNLOADEDDLG_H
#define RSSDOWNLOADEDDLG_H
#include "ui_RssDownloadedDlg.h"
#include <QDialog>

class RssDownloadedDlg : public QDialog, Ui_RssDownloadedDlg
{
Q_OBJECT
public:
	RssDownloadedDlg(QStringList* eps, const char* mask, QWidget* parent);
	int exec();
public slots:
	void add();
	void remove();
private:
	QStringList* m_episodes;
	const char* m_mask;
};

#endif
