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

#ifndef LIMITEDSOCKET_H
#define LIMITEDSOCKET_H
#include <QThread>
#include <QByteArray>
#include <QReadWriteLock>
#include <QList>
#include <QPair>
#include <QTime>
#include <QUrl>
#include <QMutexLocker>
#include <QTcpServer>
#include "fatrat.h"
#include <QFile>
#include <QSslSocket>

class LimitedSocket : public QThread
{
Q_OBJECT
public:
	LimitedSocket();
	virtual ~LimitedSocket();
	
	QByteArray readLine();
	bool readCycle();
	bool writeCycle();
	
	int speed() const;
	void destroy();
	
	inline void bind(QHostAddress addr) { m_bindAddress = addr; }
	inline qint64 done() const { return m_nTransfered+m_nResume; }
	inline void setLimit(int bytespersec) { m_nSpeedLimit = bytespersec; }
	inline QString errorString() const { return m_strError; }
	inline qulonglong total() const { return m_nToTransfer+m_nResume; }
	inline void setSegment(qint64 from, qint64 to) { m_nSegmentStart=from; m_nSegmentEnd=to; }
	
	void connectToHost(QTcpSocket* socket, QString host, quint16 port);
	void connectToHost(QSslSocket* socket, QString host, quint16 port);
	static QList<QHostAddress> performResolve(QString host, bool bPreferIPv6 = true);
	
	virtual void request(QString file, bool bUpload, int flags) = 0;
	virtual void setRemoteName(QString name) = 0;
	virtual QIODevice* getRemote() = 0;
signals:
	void finished(bool error);
	void logMessage(QString msg);
	void statusMessage(QString msg);
	void receivedSize(qint64 size);
protected:
	bool open(QString file, bool bOpenForWrite = true);
	
	static void doClose(QTcpSocket** sock);
	static bool bindSocket(QAbstractSocket* sock, QHostAddress addr);
	static QString getErrorString(QAbstractSocket::SocketError err);
protected:
	QString m_strError;
	QFile m_file;
	bool m_bAbort, m_bUpload;
	qint64 m_nTransfered, m_nToTransfer, m_nResume;
	qint64 m_nSegmentStart, m_nSegmentEnd;
	QHostAddress m_bindAddress;
private:
	int m_nSpeedLimit;
	QTime m_timer;
	qint64 m_prevBytes;
	QList<QPair<int,qulonglong> > m_stats;
	mutable QReadWriteLock m_statsLock;
};

#endif
