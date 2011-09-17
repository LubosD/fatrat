/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2009 Lubos Dolezel <lubos a dolezel.info>

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

#include "ReportBugDlg.h"
#include <QMessageBox>
#include <QUrl>
#include <QHttp>
#include <QByteArray>
#include <QRegExp>
#include <QtDebug>

ReportBugDlg::ReportBugDlg(QWidget* parent)
		: QDialog(parent)
{
	setupUi(this);
}

void ReportBugDlg::accept()
{
	QString from = lineFrom->text();
	QString sender = lineSender->text();
	QString subject = lineSubject->text();
	QString desc = textDescription->toPlainText();
	QString strData;

	if(subject.isEmpty())
	{
		QMessageBox::warning(this, "FatRat", tr("The summary is mandatory!"));
		return;
	}
	if(!QRegExp("[^@]+@[^\\.]+\\..{2,6}").exactMatch(sender))
	{
		QMessageBox::warning(this, "FatRat", tr("Your e-mail address is mandatory!"));
		return;
	}
	if(desc.isEmpty())
	{
		QMessageBox::warning(this, "FatRat", tr("The problem description is mandatory!"));
		return;
	}

	QHttpRequestHeader hdr("POST", "/report-bugs-submit");

	QHttp* http = new QHttp("fatrat.dolezel.info", 80, this);
	connect(http, SIGNAL(done(bool)), this, SLOT(done(bool)));

	hdr.addValue("Content-Type", "application/x-www-form-urlencoded; charset=utf-8");
	hdr.addValue("Host", "fatrat.dolezel.info");

	strData = "name="+mimeEncode(from)+"&sender="+QUrl::toPercentEncoding(sender)+"&subject="+mimeEncode(subject)
		  +"&text="+QUrl::toPercentEncoding(desc);

	http->request(hdr, strData.toUtf8());
}

QString ReportBugDlg::escapeUtf8(QString str)
{
	QString out;
	for(int i=0;i<str.size();i++)
	{
		if(str[i].unicode() < 128)
			out += str[i];
		else
			out += QString("&#")+QString::number(str[i].unicode())+';';
	}
	return out;
}

QByteArray ReportBugDlg::mimeEncode(QString str)
{
	QString out = "=?UTF-8?B?";
	out += str.toUtf8().toBase64() + "?=";
	return QUrl::toPercentEncoding(out);
}

void ReportBugDlg::done(bool error)
{
	if(!error)
	{
		QMessageBox::information(this, "FatRat", tr("Your bug report has been sent. Thank you for your time!"));
		QDialog::accept();
	}
	else
	{
		QMessageBox::critical(this, "FatRat", tr("Failed to send your bug report. There is either problem with your connectivity or the remote server is experiencing some issues."));
	}
}

