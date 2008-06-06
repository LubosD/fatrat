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

#ifndef FTPCLIENT_H
#define FTPCLIENT_H
#include <QTcpSocket>
#include "LimitedSocket.h"

class FtpClient : public LimitedSocket
{
Q_OBJECT
public:
	enum FtpFlags { FtpActive = 1, FtpPassive = 2};
	FtpClient(QUrl url, QUuid proxy);
	virtual void setRemoteName(QString name) { m_strName = name; }
	virtual void request(QString file, bool bUpload, int flags);
	virtual void run();
	virtual QIODevice* getRemote() { return m_pRemote; }
private:
	int readStatus(QString& line);
	void writeLine(QString line);
	
	void connectServer();
	void login();
	void requestFile();
	void appendFile();
	void switchToBinary();
	void switchToDirectory();
	void setResume();
	qint64 querySize();
	bool passiveConnect();
	bool activeConnect(QTcpServer** server);
	bool activeConnectFin(QTcpServer* server);
private:
	QTcpSocket* m_pRemote;
	QTcpSocket* m_pSocketMain;
	QString m_strUser, m_strPassword, m_strName;
	QUrl m_url;
	int m_flags;
	int m_nPort;
	Proxy m_proxyData;
};

class ActivePortAllocator : public QObject
{
Q_OBJECT
public:
	ActivePortAllocator();
	QTcpServer* getNextPort();
public slots:
	void listenerDestroyed(QObject* obj);
private:
	unsigned short m_nRangeStart;
	QMap<unsigned short, QTcpServer*> m_existing;
	QMutex m_mutex;
};

#endif
