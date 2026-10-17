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

#ifndef PYTHONACCOUNTSTATUSWIDGET_H
#define PYTHONACCOUNTSTATUSWIDGET_H

#include "config.h"
#ifndef WITH_PYPLUGINS
#	error This file is not supposed to be included!
#endif

#include <QFrame>
#include <QList>
#include <QMap>
#include <QTreeWidgetItem>

#include "python/PyRef.h"
#include "ui_PythonAccountStatusWidget.h"

/* The premium-account balances behind the status-bar icon.
 *
 * Every account-status plugin is asked at once when this opens; rows fill in
 * as the answers arrive.
 */
class PythonAccountStatusWidget : public QFrame, Ui_PythonAccountStatusWidget
{
Q_OBJECT
public:
	PythonAccountStatusWidget(QWidget* parent);
	virtual ~PythonAccountStatusWidget();

	// True when at least one account-status plugin is installed.
	static bool available();

	void accountStatusReceived(const QString& name, int state, const QString& balance);
private:
	QList<PyRef> m_running;
	QMap<QString, QTreeWidgetItem*> m_items;
};

#endif // PYTHONACCOUNTSTATUSWIDGET_H
