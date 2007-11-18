#ifndef FTPCLIENT_H
#define FTPCLIENT_H
#include <QTcpSocket>
#include "LimitedSocket.h"

class FtpEngine : public LimitedSocket
{
Q_OBJECT
public:
	enum FtpFlags { FtpActive = 1, FtpPassive = 2};
	FtpEngine(QUrl url, QUuid proxy);
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
	bool activeConnect(QTcpServer& server);
	bool activeConnectFin(QTcpServer& server);
private:
	QTcpSocket* m_pRemote;
	QTcpSocket* m_pSocketMain;
	QString m_strUser, m_strPassword, m_strName;
	QUrl m_url;
	int m_flags;
	int m_nPort;
	Proxy m_proxyData;
};

#endif
