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
*/

#include "RapidshareStatusWidget.h"
#include "Settings.h"
#include "fatrat.h"
#include <QRegExp>

RapidshareStatusWidget::RapidshareStatusWidget()
	: m_http("ssl.rapidshare.com", QHttp::ConnectionModeHttps), m_buffer(0)
{
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(refresh()));
	connect(&m_http, SIGNAL(done(bool)), this, SLOT(done(bool)));
	connect(&m_http, SIGNAL(sslErrors(const QList<QSslError>&)), &m_http, SLOT(ignoreSslErrors()));
	
	applySettings();
	m_timer.start(1000*60*10); // every 10 minutes
}

void RapidshareStatusWidget::applySettings()
{
	m_dataLogin = "login=";
	m_dataLogin += getSettingsValue("rapidshare/username").toString();
	m_dataLogin += "&password=";
	m_dataLogin += getSettingsValue("rapidshare/password").toString();
	
	refresh();
}

void RapidshareStatusWidget::refresh()
{
	m_buffer = new QBuffer(this);
	m_http.post("/cgi-bin/premiumzone.cgi", m_dataLogin, m_buffer);
}

void RapidshareStatusWidget::done(bool error)
{
	m_buffer->deleteLater();
	
	if(!error)
	{
		QRegExp re("setzeTT\\(\"\"\\+Math\\.ceil\\((\\d+)/(\\d+)");
		QRegExp re2(">\\+(\\d+) GB</a>");
		if(re.indexIn(m_buffer->data()) != -1)
		{
			qint64 bytes = re.cap(1).toDouble()/re.cap(2).toDouble()*1024LL*1024LL;
			if(re2.indexIn(m_buffer->data()) != -1)
				bytes += re2.cap(1).toLongLong()*1024LL*1024LL*1024LL;
			setText(QString("<b>RS</b>: ") + formatSize(bytes));
			return;
		}
	}
	
	setText("<b>RS</b>: <font color=red>ERROR");
}

