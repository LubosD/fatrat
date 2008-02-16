#ifndef SftpEngine_H
#define SftpEngine_H
#include "config.h"

#ifndef WITH_SFTP
#	error This file is not supposed to be included!
#endif

#include "LimitedSocket.h"
#include <QTcpSocket>
#include <QUrl>

#include <libssh2.h>
#include <libssh2_sftp.h>

class SftpEngine : public LimitedSocket
{
Q_OBJECT
public:
	SftpEngine(QUrl url);
	virtual ~SftpEngine();
	virtual void request(QString localfile, bool bUpload, int);
	virtual void setRemoteName(QString name) { m_strName = name; }
	virtual QIODevice* getRemote() { return m_pFileDevice; }
	virtual void run();
	
	QUrl m_url;
private:
	qint64 querySize(LIBSSH2_SFTP* sftp, QByteArray file);
	QString sshError();
	
	QIODevice* m_pFileDevice;
	QTcpSocket* m_pSocket;
	QString m_strName;
	
	LIBSSH2_SESSION* m_pSession;
};

class SftpFile : public QIODevice
{
public:
	SftpFile(LIBSSH2_SFTP_HANDLE* handle, qint64 mysize, QObject* parent);
	~SftpFile() { close(); }
	
	virtual bool atEnd() const;
	virtual qint64 bytesAvailable() const;
	virtual qint64 pos() const;
	virtual bool reset();
	virtual bool seek(qint64 pos);
	virtual qint64 size() const;
	virtual qint64 readData(char* data, qint64 maxSize);
	virtual qint64 writeData(const char* data, qint64 maxSize);
	virtual void close();
	virtual bool open(OpenMode mode);
	
	virtual bool waitForBytesWritten(int) { return true; }
	//virtual bool waitForReadyRead(int) { return true; }
	//virtual qint64 bytesToWrite() const { return 0; }
	virtual bool canReadLine() const { return false; }
	virtual bool isSequential() const { return true; }
private:
	LIBSSH2_SFTP_HANDLE* m_handle;
	qint64 m_nSize;
};

#endif
