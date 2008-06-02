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

#ifndef LOGGER_H
#define LOGGER_H
#include <QObject>
#include <QString>
#include <QReadWriteLock>

class Logger : public QObject
{
Q_OBJECT
public:
	QString logContents() const;
	static Logger* global() { return &m_global; }
public slots:
	void enterLogMessage(QString msg);
	void enterLogMessage(QString sender, QString msg);
signals:
	void logMessage(QString msg);
private:
	QString m_strLog;
	mutable QReadWriteLock m_lock;
	static Logger m_global;
};

#endif
