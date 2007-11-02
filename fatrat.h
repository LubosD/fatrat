#ifndef _FATRAT_H
#define _FATRAT_H
#include <QString>
#include <QThread>
#include <QUuid>
#include <QList>
#include <QVariant>

#define VERSION "SVN"

QString formatSize(qulonglong size, bool persec = false);
QString formatTime(qulonglong secs);
QVariant getSettingsDefault(QString id);
quint32 qntoh(quint32 source);
QWidget* getMainWindow();
bool recursiveRemove(QString what);

class Sleeper : public QThread
{
public:
	static void sleep(unsigned long secs) {QThread::sleep(secs);}
	static void msleep(unsigned long msecs) {QThread::msleep(msecs);}
	static void usleep(unsigned long usecs) {QThread::usleep(usecs);}
};

struct Proxy
{
	QString strName, strIP, strUser, strPassword;
	quint16 nPort;
	enum ProxyType { ProxyNone=-1, ProxyHttp, ProxySocks5 } nType;
	QUuid uuid;
	
	static QList<Proxy> loadProxys();
	static ProxyType getProxyType(QUuid uuid);
};

struct Auth
{
	QString strRegExp, strUser, strPassword;
	static QList<Auth> loadAuths();
};

enum FtpMode { FtpActive = 0, FtpPassive };

#endif
