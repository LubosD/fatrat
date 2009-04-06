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

#ifndef HELPBROWSER_H
#define HELPBROWSER_H
#include "config.h"
#include "ui_HelpBrowser.h"
#include <QWidget>
#include <QtGlobal>

#ifndef WITH_DOCUMENTATION
#	error This file is not supposed to be included!
#endif

#if QT_VERSION < 0x040400
#	error You need at least Qt 4.4 for this feature
#endif

#include <QtHelp/QtHelp>

class HelpBrowser : public QWidget, Ui_HelpBrowser
{
Q_OBJECT
public:
	HelpBrowser();
public slots:
	void openPage(const QUrl& url);
	void itemClicked(const QModelIndex& index);
	void sourceChanged();
signals:
	void changeTabTitle(QString);
private:
	QHelpEngine* m_engine;
};

#endif
