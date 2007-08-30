#include "fatrat.h"
#include <QApplication>
#include <QReadWriteLock>
#include <iostream>
#include "MainWindow.h"
#include "QueueMgr.h"
#include "Queue.h"
#include "Transfer.h"
#include "dbus_adaptor.h"

#include <QTranslator>
#include <QLocale>
#include <QtDebug>
#include <QSettings>
#include <QHttpResponseHeader>

using namespace std;

extern QueueMgr g_qmgr;
MainWindow* g_wndMain;

static void initEngines();

int main(int argc,char** argv)
{
	QApplication app(argc, argv);
	int rval;
	QTranslator translator;
	
	QCoreApplication::setOrganizationName("Dolezel");
	QCoreApplication::setOrganizationDomain("dolezel.info");
	QCoreApplication::setApplicationName("fatrat");
	
	qDebug() << "Current locale" << QLocale::system().name();
	translator.load(QString("fatrat_") + QLocale::system().name(), "/usr/share/locale/fatrat");
	app.installTranslator(&translator);
	
	// Init download engines (let them load settings)
	initEngines();
	Queue::loadQueues();
	
	qRegisterMetaType<QHttpResponseHeader>("QHttpResponseHeader");
	
	g_qmgr.start();
	g_wndMain = new MainWindow;
	
	new FatratAdaptor(g_wndMain);
	QDBusConnection::sessionBus().registerObject("/", g_wndMain);
	QDBusConnection::sessionBus().registerService("info.dolezel.fatrat");
	
	g_wndMain->showMaximized();
	app.setQuitOnLastWindowClosed(false);
	rval = app.exec();
	
	delete g_wndMain;
	
	Queue::saveQueues();
	g_qmgr.exit();
	Queue::unloadQueues();
	
	return rval;
}

static void initEngines()
{
	const EngineEntry* engines;
	
	engines = Transfer::engines(Transfer::Download);
	for(int i=0;engines[i].shortName;i++)
	{
		if(engines[i].lpfnInit)
			engines[i].lpfnInit();
	}
	
	engines = Transfer::engines(Transfer::Upload);
	for(int i=0;engines[i].shortName;i++)
	{
		if(engines[i].lpfnInit)
			engines[i].lpfnInit();
	}
}

QString formatSize(qulonglong size, bool persec)
{
	QString rval;
	
	if(size < 1024)
		rval = QString("%1 B").arg(size);
	else if(size < 1024*1024)
		rval = QString("%1 kB").arg(size/1024);
	else if(size < 1024*1024*1024)
		rval = QString("%1 MB").arg(double(size)/1024.0/1024.0, 0, 'f', 1);
	else
		rval = QString("%1 GB").arg(double(size)/1024.0/1024.0/1024.0, 0, 'f', 1);
	
	if(persec) rval += "/s";
	return rval;
}

QString formatTime(qulonglong inval)
{
	QString result;
	qulonglong days,hrs,mins,secs;
	days = inval/(60*60*24);
	inval %= 60*60*24;
	
	hrs = inval/(60*60);
	inval %= 60*60;
	
	mins = inval/60;
	secs = inval%60;
	
	if(days)
		result = QString("%1d ").arg(days);
	if(hrs)
		result += QString("%1h ").arg(hrs);
	if(mins)
		result += QString("%1m ").arg(mins);
	if(secs)
		result += QString("%1s").arg(secs);
	
	return result;
}

QList<Proxy> Proxy::loadProxys()
{
	QSettings s;
	QList<Proxy> r;
	
	int count = s.beginReadArray("httpftp/proxys");
	for(int i=0;i<count;i++)
	{
		Proxy p;
		s.setArrayIndex(i);
		
		p.strName = s.value("name").toString();
		p.strIP = s.value("ip").toString();
		p.nPort = s.value("port").toUInt();
		p.strUser = s.value("user").toString();
		p.strPassword = s.value("password").toString();
		p.nType = (Proxy::ProxyType) s.value("type",0).toInt();
		p.uuid = s.value("uuid").toString();
		
		r << p;
	}
	s.endArray();
	return r;
}

QList<Auth> Auth::loadAuths()
{
	QSettings s;
	QList<Auth> r;
	
	int count = s.beginReadArray("httpftp/auths");
	for(int i=0;i<count;i++)
	{
		Auth auth;
		s.setArrayIndex(i);
		
		auth.strRegExp = s.value("regexp").toString();
		auth.strUser = s.value("user").toString();
		auth.strPassword = s.value("password").toString();
		
		r << auth;
	}
	s.endArray();
	
	return r;
}

Proxy::ProxyType Proxy::getProxyType(QUuid uuid)
{
	QSettings s;
	QList<Proxy> r;
	
	int count = s.beginReadArray("httpftp/proxys");
	for(int i=0;i<count;i++)
	{
		Proxy p;
		s.setArrayIndex(i);
		
		if(s.value("uuid").toString() == uuid.toString())
			return (Proxy::ProxyType) s.value("type",0).toInt();
	}
	return Proxy::ProxyNone;
}

quint32 qntoh(quint32 source)
{
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
	return source;
#else

	return 0
		   | ((source & 0x000000ff) << 24)
		   | ((source & 0x0000ff00) << 8)
		   | ((source & 0x00ff0000) >> 8)
		   | ((source & 0xff000000) >> 24);
#endif
}


