/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2011 Lubos Dolezel <lubos a dolezel.info>

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

#ifndef FILESHARINGSEARCH_H
#define FILESHARINGSEARCH_H

#include <QWidget>
#include "ui_FileSharingSearch.h"

class JSearchPlugin;

class FileSharingSearch : public QWidget, Ui_FileSharingSearch
{
    Q_OBJECT
public:
	FileSharingSearch(QWidget *parent = 0);

	static void globalInit();
	static void globalExit();
	static QWidget* create() { return new FileSharingSearch; }

protected:
	void addSearchEngines();
	void enableControls(bool enable);
	void searchDone(QString cls);
	void checkIfFinished();
public slots:
	void searchClicked();
	void downloadClicked();
	void setSearchFocus();
public:
	struct SearchResult
	{
		QString name, url, extraInfo;
		qint64 fileSize;
	};
	struct SearchEngine
	{
		QString name, clsName;
		//bool finished;
	};

	void addSearchResults(QString fromClass, QList<SearchResult>& res);
	void searchFailed(QString fromClass);
private:
	static QList<SearchEngine> m_engines;
	QList<JSearchPlugin*> m_remaining;
};

#endif // FILESHARINGSEARCH_H
