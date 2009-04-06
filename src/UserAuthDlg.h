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

#ifndef _USERAUTHDLG_H
#define _USERAUTHDLG_H
#include <QDialog>
#include <QRegExp>
#include <QMessageBox>
#include "Auth.h"
#include "ui_UserAuthDlg.h"

class UserAuthDlg : public QDialog, Ui_UserAuthDlg
{
Q_OBJECT
public:
	UserAuthDlg(bool bRegExpMode, QWidget* parent)
	: QDialog(parent), m_bRegExpMode(bRegExpMode)
	{
		setupUi(this);
	}
	int exec()
	{
		int r;
		
		lineRegExp->setVisible(m_bRegExpMode);
		labelRegExp->setVisible(m_bRegExpMode);
		
		lineRegExp->setText(m_auth.strRegExp);
		lineUser->setText(m_auth.strUser);
		linePassword->setText(m_auth.strPassword);
		
		if((r = QDialog::exec()) == QDialog::Accepted)
		{
			m_auth.strRegExp = lineRegExp->text();
			m_auth.strUser = lineUser->text();
			m_auth.strPassword = linePassword->text();
		}
		return r;
	}
public slots:
	virtual void accept()
	{
		if(m_bRegExpMode)
		{
			QRegExp re (lineRegExp->text());
			if(!re.isValid())
			{
				QMessageBox::warning(this, "FatRat", tr("The regular expression you've entered is invalid."));
			}
			else if(!lineUser->text().isEmpty())
				QDialog::accept();
		}
		else
			QDialog::accept();
	}
private:
	bool m_bRegExpMode;
public:
	Auth m_auth;
};

#endif

