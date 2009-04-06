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

#ifndef _QUEUEDLG_H
#define _QUEUEDLG_H
#include <QDialog>
#include "ui_QueueDlg.h"

class QueueDlg : public QDialog, Ui_QueueDlg
{
Q_OBJECT
public:
	QueueDlg(QWidget* parent = 0);
	int exec();
public slots:
	virtual void accept();
	void limitToggled(bool checked);
	void browse();
	void browseMove();
public:
	QString m_strName, m_strDefaultDirectory, m_strMoveDirectory;
	int m_nDownLimit, m_nUpLimit, m_nDownTransfersLimit, m_nUpTransfersLimit;
	bool m_bUpAsDown, m_bLimit;
};

#endif
