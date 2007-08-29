#ifndef _SIMPLEEMAIL_H
#define _SIMPLEEMAIL_H
#include <QTcpSocket>
#include <QTextStream>

class SimpleEmail : public QObject
{
Q_OBJECT
public:
	SimpleEmail(QString server, QString from, QString to, QString message);
public slots:
	void connected();
	void readyRead();
	void error(QAbstractSocket::SocketError);
private:
	QString m_strFrom, m_strTo, m_strMessage;
	QTcpSocket* m_pSocket;
	QTextStream* m_pStream;
	enum State { Init, Mail, Rcpt, Data, Body, Quit, Close } m_state;
};

#endif
