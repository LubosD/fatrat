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

#include "CertGenDlg.h"
#include <unistd.h>

CertGenDlg::CertGenDlg(QWidget* parent) : QDialog(parent)
{
	setupUi(this);

	char hn[HOST_NAME_MAX], dn[HOST_NAME_MAX];
	if (gethostname(hn, sizeof hn) != -1)
	{
		if (getdomainname(dn, sizeof dn) != -1)
		{
			QString host = QString(hn) + '.' + dn;
			lineHostname->setText(host);
		}
	}
}

void CertGenDlg::accept()
{
	if (!lineHostname->text().isEmpty())
		QDialog::accept();
}
