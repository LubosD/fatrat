/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2009 Lubos Dolezel <lubos a dolezel.info>

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

#ifndef REPORTBUGDLG_H
#define REPORTBUGDLG_H
#include <QDialog>
#include "ui_ReportBugDlg.h"

class ReportBugDlg : public QDialog, Ui_ReportBugDlg
{
Q_OBJECT
public:
	ReportBugDlg(QWidget* parent);
	virtual void accept();
	static QString escapeUtf8(QString str);
	static QByteArray mimeEncode(QString str);
public slots:
	void done(bool error);
};

#endif // REPORTBUGDLG_H
