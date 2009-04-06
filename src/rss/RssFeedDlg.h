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

#ifndef RSSFEEDDLG_H
#define RSSFEEDDLG_H
#include <QDialog>
#include "ui_RssFeedDlg.h"

class RssFeedDlg : public QDialog, Ui_RssFeedDlg
{
Q_OBJECT
public:
	RssFeedDlg(QWidget* parent)
	: QDialog(parent)
	{
		setupUi(this);
	}
	
	int exec()
	{
		int r;
		
		lineName->setText(m_strName);
		lineURL->setText(m_strURL);
		
		if((r = QDialog::exec()) == QDialog::Accepted)
		{
			m_strName = lineName->text();
			m_strURL = lineURL->text();
		}
		return r;
	}
	
	void accept()
	{
		if(!lineName->text().isEmpty() && !lineURL->text().isEmpty())
			QDialog::accept();
	}
	
	QString m_strName, m_strURL;
};

#endif
