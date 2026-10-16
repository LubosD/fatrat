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


#include "JavaDownloadOptsForm.h"
#include "fatrat.h"
#include <QMessageBox>

WidgetHostChild* JavaDownload::createOptionsWidget(QWidget* w)
{
	return new JavaDownloadOptsForm(w, this);
}

JavaDownloadOptsForm::JavaDownloadOptsForm(QWidget* me, JavaDownload* myobj)
	: m_download(myobj)
{
	setupUi(me);
}

void JavaDownloadOptsForm::load()
{
	labelClass->setText(m_download->m_strClass);
	lineURL->setText(m_download->m_strOriginal);
}

void JavaDownloadOptsForm::accepted()
{
	QString newUrl = lineURL->text();

	if (newUrl != m_download->m_strOriginal)
	{
		m_download->m_strOriginal = newUrl;
		if (m_download->isActive())
			m_download->setState(Transfer::Waiting); // restart
	}
}

bool JavaDownloadOptsForm::accept()
{
	QString newUrl = lineURL->text();

	if (newUrl != m_download->m_strOriginal)
	{
		if (!JavaDownload::m_engines[m_download->m_strClass].regexp.match(newUrl).hasMatch())
		{
			QMessageBox::warning(getMainWindow(), "FatRat", tr("Invalid URL."));
			return false;
		}
	}
	return true;
}
