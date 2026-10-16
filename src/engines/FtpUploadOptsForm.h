/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2011 Lubos Dolezel <lubos a dolezel.info>

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


#ifndef FTPUPLOADOPTSFORM_H
#define FTPUPLOADOPTSFORM_H
#include "CurlUpload.h"
#include "WidgetHostChild.h"
#include "ui_FtpUploadOptsForm.h"

class FtpUploadOptsForm : public QObject, public WidgetHostChild, Ui_FtpUploadOptsForm
{
Q_OBJECT
public:
	FtpUploadOptsForm(QWidget* me,CurlUpload* myobj);
	virtual void load();
	virtual void accepted();
	virtual bool accept();
private:
	CurlUpload* m_upload;
};

#endif
