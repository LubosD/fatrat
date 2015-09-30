/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

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

#ifndef LOGGER_H
#define LOGGER_H
#include <QObject>
#include <QString>
#include <QReadWriteLock>

class Logger : public QObject
{
Q_OBJECT
public:
	Logger();
	~Logger();

	Q_INVOKABLE QString logContents() const;
	Q_PROPERTY(QString logContents READ logContents)
	static Logger* global() { return &m_global; }

	void toggleSysLog(bool on);
public slots:
	void enterLogMessage(QString msg);
	void enterLogMessage(QString sender, QString msg);
signals:
	void logMessage(QString msg);
private:
	QString m_strLog;
	bool m_bSysLog;
	mutable QReadWriteLock m_lock;
	static Logger m_global;
};

#endif
