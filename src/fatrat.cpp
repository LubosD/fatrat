/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2010 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 3 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
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
#include <QTextCodec>

#include <dlfcn.h>
#include <cstdlib>
#include <signal.h>
#include <unistd.h>
#include <iostream>
#include <ctime>
#include <cstring>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <errno.h>
#include <openssl/ssl.h>
#include <openssl/opensslv.h>

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
#include "dbus/NotificationsProxy.h"
#include "dbus/KNotify.h"
#include "rss/RssFetcher.h"
#include "MyApplication.h"
#include "Scheduler.h"
#include "TransferFactory.h"

#ifdef WITH_WEBINTERFACE
#	include "remote/HttpService.h"
#	include "remote/XmlRpcService.h"
#endif

#ifdef WITH_JPLUGINS
#	include "java/JVM.h"
#	include "engines/JavaDownload.h"
#	include "engines/JavaExtractor.h"
#	include "engines/JavaUpload.h"
#	include "tools/FileSharingSearch.h"
#	include "java/JClass.h"
#	include "java/JString.h"
#	include "java/JArray.h"
#	include "java/JScope.h"
#endif

using namespace std;

MainWindow* g_wndMain = 0;
QMap<QString,PluginInfo> g_plugins;

extern QVector<EngineEntry> g_enginesDownload;
extern QVector<EngineEntry> g_enginesUpload;
extern QSettings* g_settings;

static void runEngines(bool init = true);
static QString argsToArg(int argc,char** argv);
static void processSession(QString arg);
static void loadPlugins();
static void loadPlugins(const char* dir);
static void showHelp();
static void installSignalHandler();
static void testJava();
static void daemonize();
static void initDbus();
static void testNotif();
static void writePidFile();
static void dropPrivileges();

static bool m_bForceNewInstance = false;
static bool m_bStartHidden = false;
static bool m_bStartGUI = true;
static bool m_bDisableJava = false, m_bJavaForceSearch = false;
static QString m_strUnitTest;
static QString m_strSettingsPath;
static QString m_strPidFile, m_strSetUser;

static int g_argc = -1;
static char** g_argv = 0;
static QueueMgr* g_qmgr = 0;

class MyApplication;

int main(int argc,char** argv)
{
	QCoreApplication* app = 0;
	int rval;
	QString arg;
	
	qsrand(time(0));
	
	QCoreApplication::setOrganizationName("Dolezel");
	QCoreApplication::setOrganizationDomain("dolezel.info");
	QCoreApplication::setApplicationName("fatrat");
	
	arg = argsToArg(argc, argv);

	if (!m_strPidFile.isEmpty())
		writePidFile();
	if (!m_strSetUser.isEmpty())
		dropPrivileges();
	
	if (m_bStartGUI)
		app = new MyApplication(argc, argv);
	else
		app = new QCoreApplication(argc, argv);

	g_argc = argc;
	g_argv = argv;

	if(!m_bForceNewInstance)
		processSession(arg);
    
    
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	SSL_library_init();
#else
	OPENSSL_init_ssl(0, NULL);
#endif
	
#ifdef WITH_NLS
	QTranslator translator;
	{
		QString fname = QString("fatrat_") + QLocale::system().name();
		qDebug() << "Current locale" << QLocale::system().name();
		translator.load(fname, getDataFileDir("/lang", fname));
		QCoreApplication::installTranslator(&translator);
	}
#endif
	
	// Init download engines (let them load settings)
	initSettingsDefaults(m_strSettingsPath);
	
	if(m_bStartGUI)
		initSettingsPages();

#ifdef WITH_JPLUGINS
	if (!m_bDisableJava)
	{
		new JVM(m_bJavaForceSearch);

		if (JVM::JVMAvailable())
		{
			JavaDownload::globalInit();
			JavaExtractor::globalInit();
			JavaUpload::globalInit();
			FileSharingSearch::globalInit();
		}
	}
#endif
	
	installSignalHandler();
	initTransferClasses();
	loadPlugins();
	runEngines();

	qRegisterMetaType<QString*>("QString*");
	qRegisterMetaType<QByteArray*>("QByteArray*");
	qRegisterMetaType<Transfer*>("Transfer*");
	qRegisterMetaType<Transfer::TransferList>("Transfer::TransferList");

	Queue::loadQueues();
	initAppTools();

	// force singleton creation
	TransferFactory::instance();
	
	g_qmgr = new QueueMgr;

#ifdef WITH_WEBINTERFACE
	XmlRpcService::globalInit();
	new HttpService;
#endif
	
	if(m_bStartGUI)
		g_wndMain = new MainWindow(m_bStartHidden);
	else
		qDebug() << "FatRat is up and running now";
	
	new RssFetcher;
	
	initDbus();

	//testNotif();
	
	if(!arg.isEmpty() && m_bStartGUI)
		g_wndMain->addTransfer(arg);
	
	new Scheduler;
	
	if(m_bStartGUI)
		QApplication::setQuitOnLastWindowClosed(false);
	rval = app->exec();
	
#ifdef WITH_WEBINTERFACE
	delete HttpService::instance();
#endif
	delete RssFetcher::instance();
	delete Scheduler::instance();
	delete g_wndMain;
	
	g_qmgr->exit();
	Queue::stopQueues();
	Queue::saveQueues();
	Queue::unloadQueues();
	
	runEngines(false);

#ifdef WITH_JPLUGINS
	if (!m_bDisableJava && JVM::JVMAvailable())
	{
		JavaExtractor::globalExit();
		JavaDownload::globalExit();
		FileSharingSearch::globalExit();
	}
#endif
	
	delete g_qmgr;
	exitSettings();
	delete app;
	
	return rval;
}

QString argsToArg(int argc,char** argv)
{
	QString arg, prg(argv[0]);

	if (prg == "fatrat-nogui" || prg.endsWith("/fatrat-nogui"))
		m_bStartGUI = false;
	
	for(int i=1;i<argc;i++)
	{
		if(!strcasecmp(argv[i], "--force") || !strcasecmp(argv[i], "-f"))
			m_bForceNewInstance = true;
		else if(!strcasecmp(argv[i], "--hidden") || !strcasecmp(argv[i], "-i"))
			m_bStartHidden = true;
		else if(!strcasecmp(argv[i], "--nogui") || !strcasecmp(argv[i], "-n"))
			m_bStartGUI = false;
		else if(!strcasecmp(argv[i], "--help") || !strcasecmp(argv[i], "-h"))
			showHelp();
		else if(!strcasecmp(argv[i], "--daemon") || !strcasecmp(argv[i], "-d"))
		{
			m_bStartGUI = false;
			daemonize();
		}
		else if (!strcasecmp(argv[i], "--pidfile") || !strcasecmp(argv[i], "-p"))
			m_strPidFile = argv[++i];
		else if (!strcasecmp(argv[i], "--user") || !strcasecmp(argv[i], "-u"))
			m_strSetUser = argv[++i];
		else if( ( !strcasecmp(argv[i], "--test") || !strcasecmp(argv[i], "-t") ) && i+1 < argc)
			m_strUnitTest = argv[++i];
		else if(!strcasecmp(argv[i], "--no-java"))
		{
			qDebug() << "Disabling Java support";
			m_bDisableJava = true;
		}
		else if(!strcasecmp(argv[i], "--force-jre-search"))
		{
			qDebug() << "Forcing the search for Java Runtime Environment\n";
			m_bJavaForceSearch = true;
		}
		else if (!strcasecmp(argv[i], "-c") || !strcasecmp(argv[i], "--config"))
			m_strSettingsPath = argv[++i];
		else if (!strcasecmp(argv[i], "--syslog"))
			Logger::global()->toggleSysLog(true);
		else if(argv[i][0] == '-')
		{
			i++;
		}
		else
		{
			if(!arg.isEmpty())
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
	if(fileName.isEmpty())
		return QLatin1String(DATA_LOCATION) + dir;
	
	QString f = QDir::homePath() + QLatin1String(USER_PROFILE_PATH) + dir;
	if(fileName[0] != '/')
		fileName.prepend('/');
	if(QFile::exists(f + fileName))
		return f;
	else
		return QLatin1String(DATA_LOCATION) + dir;
}

QStringList listDataDir(QString path)
{
	QMap<QString,QString> rv;
	// list system-wide
	QDir dir(QLatin1String(DATA_LOCATION) + path);
	QStringList entries = dir.entryList(QDir::Files);

	foreach (QString str, entries)
		rv[str] = dir.filePath(str);

	// list user's
	dir.setPath(QDir::homePath() + USER_PROFILE_PATH + path);
	if (dir.exists())
	{
		entries = dir.entryList(QDir::Files);
		foreach (QString str, entries)
			rv[str] = dir.filePath(str);
	}

	return rv.values();
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

bool programHasGUI()
{
	return m_bStartGUI;
}

void showHelp()
{
	std::cout << "FatRat download manager (" VERSION ")\n\n"
			"Copyright (C) 2006-2011 Lubos Dolezel\n"
			"Licensed under the terms of the GNU GPL version 3 as published by the Free Software Foundation\n\n"
			"-f, --force      \tRun the program even if an instance already exists\n"
			"-i, --hidden     \tHide the GUI at startup (only if the tray icon exists)\n"
			"-n, --nogui      \tStart with no GUI at all\n"
			"-d, --daemon     \tDaemonize the application (assumes --nogui)\n"
			"-c, --config file\tUse file as settings storage\n"
			"--syslog         \tPrint global log contents to syslog\n"
			"-p, --pidfile file\tSave PID to file\n"
			"-u, --user user[:grp]\tSetuid to user, setgid to grp\n"
#ifdef WITH_JPLUGINS
			"--no-java        \tDisable support for Java extensions\n"
			//"--force-jre-search\tIgnore the cached JRE location\n"
#endif
			"-h, --help       \tShow this help\n\n"
			"If started in the GUI mode, you may pass transfers as arguments and they will be presented to the user\n";
	exit(0);
}

static void terminateHandler(int s)
{
	static int callno = 0;
	if(!callno++)
		QCoreApplication::exit();
	else
		exit(s);
}

void installSignalHandler()
{
	struct sigaction act;
	
	memset(&act, 0, sizeof(act));

	act.sa_handler = terminateHandler;
	act.sa_flags = SA_NODEFER;

	sigaction(SIGINT, &act, 0);
	sigaction(SIGTERM, &act, 0);
}

void restartApplication()
{
#ifdef WITH_WEBINTERFACE
	delete HttpService::instance();
#endif

	g_qmgr->exit();
	Queue::stopQueues();
	Queue::saveQueues();

	if (execvp(g_argv[0], g_argv) == -1)
		qDebug() << "execvp() failed: " << strerror(errno);
}

#ifdef WITH_JPLUGINS
void __attribute__ ((unused)) testJava()
{
	try
	{
		JClass system("java/lang/System");
		JObject obj = system.getStaticValue("out", JSignature::sig("java.io.PrintStream")).value<JObject>();
		obj.call("println", JSignature().addString(), JArgs() << "Hello JNI world");
	}
	catch (const RuntimeException& e)
	{
		qDebug() << e.what();
	}
	qDebug() << "End test";
}
#endif

void daemonize()
{
	daemon(true, false);
}

void testNotif()
{
	OrgFreedesktopNotificationsInterface* iface = new OrgFreedesktopNotificationsInterface(OrgFreedesktopNotificationsInterface::staticInterfaceName(), "/org/freedesktop/Notifications", QDBusConnection::sessionBus());

	if (iface->isValid())
	{
		QDBusPendingReply<uint> reply = iface->Notify("fatrat", 0, "", "Summary", "Body", QStringList(), QVariantMap(), -1);
		reply.waitForFinished();
	}

	OrgKdeKNotifyInterface* iface2 = new OrgKdeKNotifyInterface(OrgKdeKNotifyInterface::staticInterfaceName(), "/Notify", QDBusConnection::sessionBus());
	if (iface2->isValid())
	{
		iface2->event("warning", "fatrat", QVariantList(), "Summary", "Body", QByteArray(), QStringList(), 5000, 0);
	}
}

void initDbus()
{
	DbusImpl* impl = new DbusImpl;
	new FatratAdaptor(impl);

	QDBusConnection::sessionBus().registerObject("/", impl);
	QDBusConnection::sessionBus().registerService("info.dolezel.fatrat");
}

void writePidFile()
{
	QFile file(m_strPidFile);
	if (!file.open(QIODevice::WriteOnly))
	{
		Logger::global()->enterLogMessage("Cannot write the pid file");
		std::cerr << "Cannot write the pid file\n";
		exit(errno);
	}
	
	file.write(QByteArray::number(getpid()));
}

void dropPrivileges()
{
	QByteArray user, group;
	int p = m_strSetUser.indexOf(':');

	if (p != -1)
	{
		user = m_strSetUser.left(p).toLatin1();
		group = m_strSetUser.mid(p+1).toLatin1();
	}
	else
		user = m_strSetUser.toLatin1();

	if (!group.isEmpty())
	{
		struct group* g = getgrnam(group.constData());
		if (!g)
		{
			Logger::global()->enterLogMessage("Cannot getgrnam() - invalid group name?");
			std::cerr << "Cannot getgrnam() - invalid group name?\n";
			exit(errno);
		}
		else if (setgid(g->gr_gid) == -1)
		{
			Logger::global()->enterLogMessage("Cannot setgid()");
			std::cerr << "Cannot setgid()\n";
			exit(errno);
		}
	}

	struct passwd* u = getpwnam(user.constData());
	if (!u)
	{
		Logger::global()->enterLogMessage("Cannot getpwnam() - invalid user name?");
		std::cerr << "Cannot getpwnam() - invalid group name?\n";
		exit(errno);
	}
	else if (setuid(u->pw_uid) == -1)
	{
		Logger::global()->enterLogMessage("Cannot setuid()");
		std::cerr << "Cannot setuid()\n";
		exit(errno);
	}
	else
	{
		setenv("HOME", u->pw_dir, true);
		chdir(u->pw_dir);
	}
}
