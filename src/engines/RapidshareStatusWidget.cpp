/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

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

#include "RapidshareStatusWidget.h"
#include "Settings.h"
#include "fatrat.h"
#include <ctime>
#include <QNetworkReply>

RapidshareStatusWidget::RapidshareStatusWidget()
{
	m_network = new QNetworkAccessManager(this);

	connect(m_network, SIGNAL(finished(QNetworkReply*)), this, SLOT(done(QNetworkReply*)));
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(refresh()));
	
	applySettings();
	m_timer.start(1000*60*10); // every 10 minutes
}

void RapidshareStatusWidget::applySettings()
{
	m_strUser = getSettingsValue("rapidshare/username").toString();
	m_strPassword = getSettingsValue("rapidshare/password").toString();
	
	refresh();
}

void RapidshareStatusWidget::refresh()
{
	QString url = QString("http://api.rapidshare.com/cgi-bin/rsapi.cgi?sub=getaccountdetails_v1&login=%1&password=%2").arg(m_strUser).arg(m_strPassword);
	m_network->get(QNetworkRequest(url));
}

void RapidshareStatusWidget::done(QNetworkReply* reply)
{
	reply->deleteLater();
	
	if (reply->error() != QNetworkReply::NoError)
		setText(QString("<b>RS</b>: <font color=red>%1").arg(tr("Network error")));
	else
	{
		QByteArray arr = reply->readAll();
		if (arr.startsWith("ERROR"))
			setText(QString("<b>RS</b>: <font color=red>%1").arg(tr("Login error")));
		else
		{
			QList<QByteArray> pairs = arr.split('\n');
			QMap<QString,QString> values;

			foreach (QByteArray pair, pairs)
			{
				int pos = pair.indexOf('=');
				if (pos < 0)
					continue;
				values[pair.left(pos)] = QString(pair.mid(pos+1)).trimmed();
			}

			if (!values.contains("billeduntil"))
			{
				setText(QString("<b>RS</b>: <font color=red>%1").arg(tr("Unknown error")));
				return;
			}

			time_t tt = values["billeduntil"].toLongLong() - time(0);
			qint64 bytes = values["tskb"].toLongLong() * 1024;

			QString text = "<b>RS</b>: ";
			text += formatSize(bytes);

			if (tt < 60*60*24*20) // less than 20 days
			{
				text += QString(" [<font color=red>%L1 %2</font>]").arg(double(tt)/(60*60*24), 0, 'f', 1).arg(tr("days"));
			}
			setText(text);
		}
	}
}

