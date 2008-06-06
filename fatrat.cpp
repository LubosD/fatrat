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

#include "config.h"
#include "fatrat.h"

#include <QApplication>
#include <QMessageBox>
#include <QTranslator>
#include <QLocale>
#include <QtDebug>
#include <QVariant>
#include <QSettings>
#include <QMap>
#include <QDir>

#include <dlfcn.h>
#include <cstdlib>

#include "MainWindow.h"
#include "QueueMgr.h"
#include "Queue.h"
#include "Transfer.h"
#include "AppTools.h"
#include "Settings.h"
#include "config.h"
#include "RuntimeException.h"
#include "dbus/DbusAdaptor.h"
#include "dbus/DbusImpl.h"
#include "rss/RssFetcher.h"

#ifdef WITH_REMOTE
#	include "remote/HttpService.h"
#endif

#ifdef WITH_JABBER
#	include "remote/JabberService.h"
#endif

using namespace std;

MainWindow* g_wndMain = 0;
QMap<QString,PluginInfo> g_plugins;

extern QVector<EngineEntry> g_enginesDownload;
extern QVector<EngineEntry> g_enginesUpload;
extern QSettings* g_settings;

const char* USER_PROFILE_PATH = "/.local/share/fatrat";

static void runEngines(bool init = true);
static QString argsToArg(int argc,char** argv);
static void processSession(QString arg);
static void loadPlugins();
static void loadPlugins(const char* dir);

static bool m_bForceNewInstance = false;
static bool m_bStartHidden = false;

int main(int argc,char** argv)
{
	QApplication app(argc, argv);
	int rval;
	QueueMgr* qmgr;
	QString arg = argsToArg(argc, argv);
	
	QCoreApplication::setOrganizationName("Dolezel");
	QCoreApplication::setOrganizationDomain("dolezel.info");
	QCoreApplication::setApplicationName("fatrat");
	
	if(!m_bForceNewInstance)
		processSession(arg);
	
#ifdef WITH_NLS
	QTranslator translator;
	{
		QString fname = QString("fatrat_") + QLocale::system().name();
		qDebug() << "Current locale" << QLocale::system().name();
		translator.load(fname, getDataFileDir("/lang", fname));
		app.installTranslator(&translator);
	}
#endif
	
	// Init download engines (let them load settings)
	initSettingsDefaults();
	initSettingsPages();
	loadPlugins();
	initTransferClasses();
	runEngines();
	Queue::loadQueues();
	initAppTools();
	
	qmgr = new QueueMgr;
	//qmgr->start();
	
	g_wndMain = new MainWindow(m_bStartHidden);
	
#ifdef WITH_REMOTE
	new HttpService;
#endif
	new RssFetcher;
	
	DbusImpl* impl = new DbusImpl;
	new FatratAdaptor(impl);
	
	QDBusConnection::sessionBus().registerObject("/", impl);
	QDBusConnection::sessionBus().registerService("info.dolezel.fatrat");
	
	if(!arg.isEmpty())
		g_wndMain->addTransfer(arg);
	
#ifdef WITH_JABBER
	new JabberService;
#endif
	
	app.setQuitOnLastWindowClosed(false);
	rval = app.exec();
	
#ifdef WITH_JABBER
	delete JabberService::instance();
#endif
#ifdef WITH_REMOTE
	delete HttpService::instance();
#endif
	delete RssFetcher::instance();
	delete g_wndMain;
	
	Queue::saveQueues();
	qmgr->exit();
	Queue::unloadQueues();
	
	runEngines(false);
	
	delete qmgr;
	exitSettings();
	
	return rval;
}

QString argsToArg(int argc,char** argv)
{
	QString arg;
	
	for(int i=1;i<argc;i++)
	{
		if(!strcasecmp(argv[i], "--force"))
			m_bForceNewInstance = true;
		else if(!strcasecmp(argv[i], "--hidden"))
			m_bStartHidden = true;
		else
		{
			if(i > 1)
				arg += '\n';
			arg += argv[i];
		}
	}
	
	return arg;
}

void processSession(QString arg)
{
	QDBusConnection conn = QDBusConnection::sessionBus();
	QDBusConnectionInterface* bus = conn.interface();
	
	if(bus->isServiceRegistered("info.dolezel.fatrat"))
	{
		qDebug() << "FatRat is already running";
		
		if(!arg.isEmpty())
		{
			qDebug() << "Passing arguments to an existing instance.";
			QDBusInterface iface("info.dolezel.fatrat", "/", "info.dolezel.fatrat", conn);
			
			iface.call("addTransfers", arg);
		}
		else
		{
			QMessageBox::critical(0, "FatRat", QObject::tr("There is already a running instance.\n"
					"If you want to start FatRat anyway, pass --force among arguments."));
		}
		
		exit(0);
	}
}

static void runEngines(bool init)
{
	for(int i=0;i<g_enginesDownload.size();i++)
	{
		if(init)
		{
			if(g_enginesDownload[i].lpfnInit)
				g_enginesDownload[i].lpfnInit();
		}
		else
		{
			if(g_enginesDownload[i].lpfnExit)
				g_enginesDownload[i].lpfnExit();
		}
	}
	
	for(int i=0;i<g_enginesUpload.size();i++)
	{
		if(init)
		{
			if(g_enginesUpload[i].lpfnInit)
				g_enginesUpload[i].lpfnInit();
		}
		else
		{
			if(g_enginesUpload[i].lpfnExit)
				g_enginesUpload[i].lpfnExit();
		}
	}
}

QWidget* getMainWindow()
{
	return g_wndMain;
}

QString formatSize(qulonglong size, bool persec)
{
	QString rval;
	
	if(size < 1024)
		rval = QString("%L1 B").arg(size);
	else if(size < 1024*1024)
		rval = QString("%L1 KB").arg(size/1024);
	else if(size < 1024*1024*1024)
		rval = QString("%L1 MB").arg(double(size)/1024.0/1024.0, 0, 'f', 1);
	else
		rval = QString("%L1 GB").arg(double(size)/1024.0/1024.0/1024.0, 0, 'f', 1);
	
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
	QList<Proxy> r;
	
	int count = g_settings->beginReadArray("httpftp/proxys");
	for(int i=0;i<count;i++)
	{
		Proxy p;
		g_settings->setArrayIndex(i);
		
		p.strName = g_settings->value("name").toString();
		p.strIP = g_settings->value("ip").toString();
		p.nPort = g_settings->value("port").toUInt();
		p.strUser = g_settings->value("user").toString();
		p.strPassword = g_settings->value("password").toString();
		p.nType = (Proxy::ProxyType) g_settings->value("type",0).toInt();
		p.uuid = g_settings->value("uuid").toString();
		
		r << p;
	}
	g_settings->endArray();
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

Proxy::Proxy Proxy::getProxy(QUuid uuid)
{
	int count = g_settings->beginReadArray("httpftp/proxys");
	for(int i=0;i<count;i++)
	{
		Proxy p;
		g_settings->setArrayIndex(i);
		
		p.uuid = g_settings->value("uuid").toString();
		if(p.uuid != uuid)
			continue;
		
		p.strName = g_settings->value("name").toString();
		p.strIP = g_settings->value("ip").toString();
		p.nPort = g_settings->value("port").toUInt();
		p.strUser = g_settings->value("user").toString();
		p.strPassword = g_settings->value("password").toString();
		p.nType = (Proxy::ProxyType) g_settings->value("type",0).toInt();
		
		g_settings->endArray();
		return p;
	}
	
	g_settings->endArray();
	return Proxy();
}

Proxy::operator QNetworkProxy() const
{
	QNetworkProxy p;
	
	if(nType == ProxyNone)
		p.setType(QNetworkProxy::NoProxy);
	else if(nType == ProxyNone)
		p.setType(QNetworkProxy::HttpProxy);
	else
		p.setType(QNetworkProxy::Socks5Proxy);
	
	p.setHostName(strIP);
	p.setUser(strUser);
	p.setPassword(strUser);
	
	return p;
}

/////////////////////////////////////////////////////////

class RecursiveRemove : public QThread
{
public:
	RecursiveRemove(QString what) : m_what(what)
	{
		start();
	}
	void run()
	{
		connect(this, SIGNAL(finished()), this, SLOT(deleteLater()));
		work(m_what);
	}
	static bool work(QString what)
	{
		qDebug() << "recursiveRemove" << what;
		if(!QFile::exists(what))
			return true; // silently ignore
		if(!QFile::remove(what))
		{
			QDir dir(what);
			if(!dir.exists())
			{
				qDebug() << "Not a directory:" << what;
				return false;
			}
			
			QStringList contents;
			contents = dir.entryList();
			
			foreach(QString item, contents)
			{
				if(item != "." && item != "..")
				{
					if(!work(dir.filePath(item)))
						return false;
				}
			}
			
			QString name = dir.dirName();
			if(!dir.cdUp())
			{
				qDebug() << "Cannot cdUp:" << what;
				return false;
			}
			if(!dir.rmdir(name))
			{
				qDebug() << "Cannot rmdir:" << name;
				return false;
			}
		}
		return true;
	}
private:
	QString m_what;
};

void recursiveRemove(QString what)
{
	new RecursiveRemove(what);
}

bool openDataFile(QFile* file, QString filePath)
{
	if(filePath[0] != '/')
		filePath.prepend('/');
	
	file->setFileName(QDir::homePath() + QLatin1String(USER_PROFILE_PATH) + filePath);
	if(file->open(QIODevice::ReadOnly))
		return true;
	file->setFileName(QLatin1String(DATA_LOCATION) + filePath);
	if(!file->open(QIODevice::ReadOnly))
	{
		Logger::global()->enterLogMessage(QObject::tr("Unable to load a data file:") + ' ' + filePath);
		return false;
	}
	else
		return true;
}

QString getDataFileDir(QString dir, QString fileName)
{
	QString f = QDir::homePath() + QLatin1String(USER_PROFILE_PATH) + dir;
	if(fileName[0] != '/')
		fileName.prepend('/');
	if(QFile::exists(f + fileName))
		return f;
	else
		return QLatin1String(DATA_LOCATION) + dir;
}

void loadPlugins()
{
	loadPlugins(PLUGIN_LOCATION);
	
	if(const char* p = getenv("PLUGIN_PATH"))
		loadPlugins(p);
}

void loadPlugins(const char* p)
{
	QDir dir(p);
	QStringList plugins = dir.entryList(QStringList("*.so"));
	
	foreach(QString pl, plugins)
	{
		QByteArray p = dir.filePath(pl).toUtf8();
		qDebug() << "dlopen" << p;
		if(void* l = dlopen(p.constData(), RTLD_NOW))
		{
			Logger::global()->enterLogMessage(QObject::tr("Loaded a plugin:") + ' ' + pl);
			
			PluginInfo (*info)() = (PluginInfo(*)()) dlsym(l, "getInfo");
			if(info != 0)
			{
				PluginInfo i = info();
				g_plugins[pl] = i;
				
				if(strcmp(i.version, VERSION))
				{
					qDebug() << "WARNING! Version conflict." << pl << "is" << i.version << "but FatRat is" << VERSION;
					Logger::global()->enterLogMessage(QObject::tr("WARNING: the plugin is incompatible:") + ' ' + pl);
				}
			}
		}
		else
			Logger::global()->enterLogMessage(QObject::tr("Failed to load a plugin: %1: %2").arg(pl).arg(dlerror()));
	}
}

