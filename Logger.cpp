#include "Logger.h"
#include <QDate>
#include <QTime>

Logger Logger::m_global;

void Logger::enterLogMessage(QString msg)
{
	QWriteLocker l(&m_lock);
	
	QString text = QString("%1 %2 - %3")
		.arg( QDate::currentDate().toString(Qt::ISODate) )
		.arg( QTime::currentTime().toString(Qt::ISODate) )
		.arg(msg);
	emit logMessage(text);
	if(!m_strLog.isEmpty())
		m_strLog += '\n';
	
	m_strLog += text;
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
