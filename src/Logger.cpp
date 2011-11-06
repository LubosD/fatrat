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

#include "Logger.h"
#include <QDate>
#include <QTime>
#include <syslog.h>

Logger Logger::m_global;

Logger::Logger()
	: m_bSysLog(false)
{
}

Logger::~Logger()
{
	if (m_bSysLog)
		closelog();
}

void Logger::toggleSysLog(bool on)
{
	if (on != m_bSysLog)
	{
		if (on)
			openlog("fatrat", LOG_PID, LOG_USER);
		else
			closelog();
		m_bSysLog = on;
	}
}

void Logger::enterLogMessage(QString msg)
{
	QWriteLocker l(&m_lock);
	
	QString text = QString("%1 %2 - %3")
		.arg( QDate::currentDate().toString(Qt::ISODate) )
		.arg( QTime::currentTime().toString(Qt::ISODate) )
		.arg(msg);

	if(!m_strLog.isEmpty())
		m_strLog += '\n';
	
	m_strLog += text;
	
	if (m_strLog.size() > 2*1024*1024)
		m_strLog = m_strLog.mid(1024*1024);

	l.unlock();
	emit logMessage(text);

	if (m_bSysLog)
	{
		std::string str = text.toStdString();
		syslog(LOG_INFO, str.c_str());
	}
}

void Logger::enterLogMessage(QString sender, QString msg)
{
	enterLogMessage(QString("[%1]: %2").arg(sender).arg(msg));
}

QString Logger::logContents() const
{
	QReadLocker l(&m_lock); // TODO: Is this sufficient?
	return m_strLog;
}
