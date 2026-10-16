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

#include "PythonDownloadOptsForm.h"

#include <QMessageBox>

#include "fatrat.h"

PythonDownloadOptsForm::PythonDownloadOptsForm(QWidget* me, PythonDownload* myobj)
	: m_download(myobj)
{
	setupUi(me);
}

void PythonDownloadOptsForm::load()
{
	labelClass->setText(m_download->myClass());
	lineURL->setText(m_download->remoteURI());
}

void PythonDownloadOptsForm::accepted()
{
	QString newUrl = lineURL->text();

	if (newUrl != m_download->remoteURI())
	{
		m_download->init(newUrl, m_download->object());

		if (m_download->isActive())
			m_download->setState(Transfer::Waiting); // restart with the new link
	}
}

bool PythonDownloadOptsForm::accept()
{
	QString newUrl = lineURL->text();

	if (newUrl == m_download->remoteURI())
		return true;

	// Only check what we can: a plugin with its own acceptable() gets asked,
	// otherwise the pattern decides.
	if (PythonDownload::acceptableFor(m_download->myClass(), newUrl) == 0)
	{
		QMessageBox::warning(getMainWindow(), "FatRat", tr("Invalid URL."));
		return false;
	}

	return true;
}
