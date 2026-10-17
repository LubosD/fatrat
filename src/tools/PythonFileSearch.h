/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2026 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 3 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#ifndef PYTHONFILESEARCH_H
#define PYTHONFILESEARCH_H

#include "config.h"
#ifndef WITH_PYPLUGINS
#	error This file is not supposed to be included!
#endif

#include <QList>
#include <QString>
#include <QWidget>

#include "python/PyRef.h"
#include "ui_PythonFileSearch.h"

/* "File sharing server search": asks every selected search plugin at once
 * and pours the answers into one list.
 */
class PythonFileSearch : public QWidget, Ui_PythonFileSearch
{
Q_OBJECT
public:
	PythonFileSearch(QWidget* parent = 0);
	virtual ~PythonFileSearch();

	static QWidget* create() { return new PythonFileSearch; }
	// True when at least one search plugin is installed.
	static bool available();

	struct SearchResult
	{
		SearchResult() : fileSize(0) {}

		QString name, url, extra;
		qint64 fileSize;
	};
public slots:
	void searchClicked();
	void downloadClicked();
	void setSearchFocus();
protected:
	void addSearchEngines();
	void enableControls(bool enable);
	void addResults(const QString& engineName, const QList<SearchResult>& results);
	void searchFinished();
private:
	// Plugin instances for the search in flight; dropped when it ends.
	QList<PyRef> m_running;
	int m_nPending;
};

#endif // PYTHONFILESEARCH_H
