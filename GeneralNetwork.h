#ifndef _GENERALNETWORK_H
#define _GENERALNETWORK_H
#include <QTcpSocket>
#include <QThread>
#include <QHttpRequestHeader>
#include <QHttpResponseHeader>
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

class LimitedSocket : public QThread
{
Q_OBJECT
public:
	LimitedSocket() : m_pSocket(0), m_bAbort(false), m_nTransfered(0), m_nToTransfer(0), m_nResume(0), m_nSpeedLimit(0) {}
	~LimitedSocket() { wait(); }
	void bind(QHostAddress addr) { m_bindAddress = addr; }
	
	QByteArray readLine();
	bool readCycle();
	bool writeCycle();
	
	int speed() const;
	void reset();
	void destroy();
	qint64 done() const { return m_nTransfered+m_nResume; }
	
	void setLimit(int bytespersec) { m_nSpeedLimit = bytespersec; }
	int limit() const { return m_nSpeedLimit; }
	
	QString errorString() const { return m_strError; }
	qulonglong total() const { return m_nToTransfer+m_nResume; }
signals:
	void finished(void*,bool error);
protected:
	bool request(QString file, bool bOpenForWrite = true);
	static void doClose(QTcpSocket*& sock);
	static bool bindSocket(QAbstractSocket* sock, QHostAddress addr);
protected:
	QString m_strError;
	QTcpSocket* m_pSocket;
	QFile m_file;
	bool m_bAbort;
	qint64 m_nTransfered, m_nToTransfer, m_nResume;
	QHostAddress m_bindAddress;
private:
	int m_nSpeedLimit;
	QTime m_timer;
	qint64 m_prevBytes;
	QList<QPair<int,qulonglong> > m_stats;
	mutable QReadWriteLock m_statsLock;
};

class HttpEngine : public LimitedSocket
{
Q_OBJECT
public:
	HttpEngine(QUrl url, QUrl referrer, QUuid proxy);
	void request(QString file);
	virtual void run();
signals:
	void responseReceived(QHttpResponseHeader resp);
private:
	QUrl m_url;
	QHttpRequestHeader m_header;
	Proxy m_proxyData;
};

class FtpEngine : public LimitedSocket
{
Q_OBJECT
public:
	enum FtpFlags { FtpActive = 1, FtpPassive = 2, FtpGet = 4, FtpPut = 8 };
	FtpEngine(QUrl url, QUuid proxy);
	void setRemoteName(QString name) { m_strName = name; }
	void request(QString file, int flags);
	virtual void run();
	int readStatus(QString& line);
	void writeLine(QString line);
signals:
	void status(QString);
	void logMessage(QString);
	void responseReceived(qulonglong totalsize);
private:
	void connectServer();
	void login();
	void requestFile();
	void appendFile();
	void switchToBinary();
	void switchToDirectory();
	void setResume();
	qint64 querySize();
	bool passiveConnect();
	bool activeConnect(QTcpServer& server);
	bool activeConnectFin(QTcpServer& server);
private:
	QTcpSocket* m_pSocketMain;
	QString m_strUser, m_strPassword, m_strName;
	QUrl m_url;
	int m_flags;
	int m_nPort;
	Proxy m_proxyData;
};

#endif
