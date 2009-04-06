/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation
with the OpenSSL special exemption.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef DBUSIMPL_H
#define DBUSIMPL_H
#include <QObject>
#include <QStringList>

class DbusImpl : public QObject
{
Q_OBJECT
public:
	DbusImpl();
	static DbusImpl* instance() { return m_instance; }
public slots:
	void addTransfers(QString uris);
	QString addTransfersNonInteractive(QString uris, QString target, QString className, int queueID);
	
	// workaround for QHttp Qt bug - receiving side
	void addTransfersNonInteractive2(QString uris, QString target, QString className, int queueID, QString* resp);
	QStringList getQueues();
public:
	// workaround for QHttp Qt bug - emiting side
	QString addTransfers(QString uris, QString target, QString className, int queueID);
private:
	static DbusImpl* m_instance;
};

#endif
