#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H
#include <QTcpSocket>
#include <QHttpRequestHeader>
#include "LimitedSocket.h"

class HttpEngine : public LimitedSocket
{
Q_OBJECT
public:
	HttpEngine(QUrl url, QUrl referrer, QUuid proxy);
	virtual void request(QString file, bool, int);
	virtual void run();
	virtual void setRemoteName(QString) { }
	virtual QIODevice* getRemote() { return m_pRemote; }
signals:
	void redirected(QString newurl);
private:
	void dataCycle(bool bChunked);
	QTcpSocket* m_pRemote;
	
	QUrl m_url;
	QHttpRequestHeader m_header;
	Proxy m_proxyData;
};

#endif
