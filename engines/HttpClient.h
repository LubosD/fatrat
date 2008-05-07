#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H
#include <QTcpSocket>
#include <QHttpRequestHeader>
#include <QMap>
#include "LimitedSocket.h"

class HttpEngine : public LimitedSocket
{
Q_OBJECT
public:
	HttpEngine(QUrl url, QUrl referrer, QUuid proxy);
	void addHeaderValue(QString name, QString value) { m_header.addValue(name, value); }
	
	// uploads only
	void setRequestBody(QByteArray hdr, QByteArray footer) { m_strHeader = hdr; m_strFooter = footer; }
	QByteArray getResponseBody() const { return m_strResponse; }
	
	virtual void request(QString file, bool bUpload, int);
	virtual void run();
	virtual void setRemoteName(QString) { }
	virtual QIODevice* getRemote() { return m_pRemote; }
	
	QMap<QString, QString>& getCookies() { return m_mapCookies; }
signals:
	void redirected(QString newurl);
	void renamed(QString dispName);
private:
	void processServerResponse();
	void dataCycle(bool bChunked);
	void handleDownloadHeaders(QHttpResponseHeader headers);
	void handleUploadHeaders(QHttpResponseHeader headers);
	void performUpload();
	
	QTcpSocket* m_pRemote;
	QUrl m_url;
	QHttpRequestHeader m_header;
	Proxy m_proxyData;
	QByteArray m_strHeader, m_strFooter, m_strResponse;
	QMap<QString, QString> m_mapCookies;
};

#endif
