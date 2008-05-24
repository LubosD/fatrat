#ifndef DATAPOLLER_H
#define DATAPOLLER_H
#include <QThread>
#include <QMap>
#include <QPair>
#include <sys/time.h>

struct TransferInfo;

static const int ERR_CONNECTION_LOST = -1;
static const int ERR_CONNECTION_ERR = -2;
static const int ERR_CONNECTION_TIMEOUT = -3;

class DataPoller : public QThread
{
Q_OBJECT
public:
	DataPoller();
	~DataPoller();
	void run();
	
	void addSocket(int socket, int file, bool bUpload, qint64 toTransfer);
	void removeSocket(int sock);
	int processRead(TransferInfo& info);
	int processWrite(TransferInfo& info);
	qint64 getSpeed(int sock);
	qint64 getProgress(int sock);
	void setSpeedLimit(int sock, qint64 limit);
	
	static DataPoller* instance() { return m_instance; }
signals:
	void processingDone(int socket, int error);
private:
	int m_epoll;
	
	QMap<int, TransferInfo> m_sockets;
	bool m_bAbort;
	
	static DataPoller* m_instance;
};

struct TransferInfo
{
	bool bUpload;
	int socket, file;
	qint64 toTransfer;
	qint64 speedLimit;
	
	// internal
	timeval lastProcess; // last reception/send
	qint64 transferred, transferredPrev;
	QList<QPair<int, qint64> > speedData; // msec, bytes
	qint64 speed;
};

#endif
