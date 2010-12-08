/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2010 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#include "CaptchaQt.h"
#include "fatrat.h"
#include "CaptchaQtDlg.h"
#include <cassert>

CaptchaQt::CaptchaQt()
{
	Captcha::registerCaptchaDecoder(this);
}

bool CaptchaQt::process(int id, QString url)
{
	if (!programHasGUI())
		return false;

	QMetaObject::invokeMethod(static_cast<QObject*>(this), "showDialog", Qt::QueuedConnection, Q_ARG(int, id), Q_ARG(QString, url));
	return true;
}

void CaptchaQt::showDialog(int id, QString url)
{
	CaptchaQtDlg* dlg = new CaptchaQtDlg;
	m_dlgs[dlg] = id;

	dlg->load(url);

	connect(dlg, SIGNAL(captchaEntered(QString)), this, SLOT(captchaEntered(QString)));
	dlg->show();
}

void CaptchaQt::captchaEntered(QString text)
{
	CaptchaQtDlg* dlg = static_cast<CaptchaQtDlg*>(sender());

	assert(m_dlgs.contains(dlg));

	int id = m_dlgs[dlg];
	m_dlgs.remove(dlg);

	dlg->hide();
	dlg->deleteLater();

	returnResult(id, text);
}

void CaptchaQt::abort(int id)
{
	qDebug() << "CaptchaQt::abort():" << id;
	for(QMap<CaptchaQtDlg*,int>::iterator it = m_dlgs.begin(); it != m_dlgs.end();)
	{
		if (it.value() == id)
		{
			it.key()->deleteLater();
			it = m_dlgs.erase(it);
		}
		else
			it++;
	}
}
