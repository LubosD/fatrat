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

#include <QCoreApplication>
#ifdef WITH_GUI
#	include <QApplication>
#	include <QMessageBox>
#endif
#include <QTranslator>
#include <QLocale>
#include <QtDebug>
#include <QVariant>
#include <QSettings>
#include <QMap>
#include <QDir>

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

#include "QueueMgr.h"
#include "Queue.h"
#include "Transfer.h"
#include "Settings.h"
#include "config.h"
#include "RuntimeException.h"
#ifdef WITH_CURL
#	include "engines/MetalinkDownload.h"
#endif
#ifdef WITH_GUI
#	include "MainWindow.h"
#	include "AppTools.h"
#	include "MyApplication.h"
#endif
#ifdef WITH_DBUS
#	include <QtDBus/QtDBus>
#	include "dbus/DbusAdaptor.h"
#	include "dbus/DbusImpl.h"
#	include "dbus/NotificationsProxy.h"
#	include "dbus/KNotify.h"
#endif
#include "rss/RssFetcher.h"
#include "watch/WatchFolderQueues.h"
#include "Scheduler.h"
#include "TransferFactory.h"
#include "plugins/TaskManager.h"
#include "plugins/PluginLoader.h"

#ifdef WITH_WEBINTERFACE
#	include "remote/HttpService.h"
#endif

#ifdef WITH_PYPLUGINS
#	include "python/PluginRegistry.h"
#	include "python/PyVM.h"
#endif


using namespace std;

#ifdef WITH_GUI
MainWindow* g_wndMain = 0;
#endif
QMap<QString,PluginInfo> g_plugins;
#ifdef WITH_PYPLUGINS
static PyVM* g_pyvm = 0;
#endif

extern QVector<EngineEntry> g_enginesDownload;
extern QVector<EngineEntry> g_enginesUpload;
extern QSettings* g_settings;

static void runEngines(bool init = true);
static QString argsToArg(int argc,char** argv);
#ifdef WITH_DBUS
static void processSession(QString arg);
#endif
static void loadPlugins();
static void loadPlugins(const char* dir);
static void showHelp();
static void installSignalHandler();
static void daemonize();
#ifdef WITH_DBUS
static void initDbus();
static void testNotif();
#endif
static void writePidFile();
static void dropPrivileges();
static void removePidFile();

static bool m_bForceNewInstance = false;
static bool m_bStartHidden = false;
static bool m_bStartGUI = true;
static bool m_bDisablePython = false;
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
	
	QCoreApplication::setOrganizationName("Dolezel");
	QCoreApplication::setOrganizationDomain("dolezel.info");
	QCoreApplication::setApplicationName("fatrat");
	
	arg = argsToArg(argc, argv);

	if (!m_strPidFile.isEmpty())
		writePidFile();
	if (!m_strSetUser.isEmpty())
		dropPrivileges();
	
#ifdef WITH_GUI
	if (m_bStartGUI)
		app = new MyApplication(argc, argv);
	else
#endif
		app = new QCoreApplication(argc, argv);

	g_argc = argc;
	g_argv = argv;

#ifdef WITH_DBUS
	if(!m_bForceNewInstance)
		processSession(arg);
#endif
    
    
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
		if(!translator.load(fname, getDataFileDir("/lang", fname)))
			qDebug() << "No translation found for" << fname;
		QCoreApplication::installTranslator(&translator);
	}
#endif
	
	// Init download engines (let them load settings)
	initSettingsDefaults(m_strSettingsPath);
	
	initSettingsPages();

#ifdef WITH_PYPLUGINS
	// Says so even with --no-python: the jars are dead either way.
	PluginRegistry::reportLegacyJavaPlugins();

	if (!m_bDisablePython)
	{
		g_pyvm = new PyVM;

		if (PyVM::available())
			PluginRegistry::globalInit();
	}
#endif

	
	// Before any plugin is loaded: TaskManager lives on the thread that first uses it,
	// and its aboutToQuit connection (cancelAll) must come before the plugins' own, so
	// that tasks are cancelled before a plugin joins its threads.
	TaskManager::instance();
	installSignalHandler();
	initTransferClasses();
	loadPlugins();
	runEngines();

	qRegisterMetaType<QString*>("QString*");
	qRegisterMetaType<QByteArray*>("QByteArray*");
	qRegisterMetaType<Transfer*>("Transfer*");
	qRegisterMetaType<Transfer::TransferList>("Transfer::TransferList");

	const bool queuesLoaded = Queue::loadQueues();
#ifdef WITH_CURL
	// With queues.xml unreadable (say, truncated by a crash and about to be restored from a backup),
	// every copy would look unused.
	if (queuesLoaded)
		MetalinkDownload::pruneSourceCopies();
#else
	Q_UNUSED(queuesLoaded);
#endif
#ifdef WITH_GUI
	initAppTools();
#endif

	// force singleton creation
	TransferFactory::instance();
	
	g_qmgr = new QueueMgr;

#ifdef WITH_WEBINTERFACE
	new HttpService;
#endif
	
#ifdef WITH_GUI
	if(m_bStartGUI)
		g_wndMain = new MainWindow(m_bStartHidden);
	else
#endif
		qDebug() << "FatRat is up and running now";
	
	new RssFetcher;
	new WatchFolderService(WatchFolderQueues::hooks());
	
#ifdef WITH_DBUS
	initDbus();
#endif

	//testNotif();
	
#ifdef WITH_GUI
	if(!arg.isEmpty() && m_bStartGUI)
		g_wndMain->addTransfer(arg);
#endif
	
	new Scheduler;
	
#ifdef WITH_GUI
	if(m_bStartGUI)
		QApplication::setQuitOnLastWindowClosed(false);
#endif
	rval = app->exec();
	
#ifdef WITH_WEBINTERFACE
	delete HttpService::instance();
#endif
	delete RssFetcher::instance();
	delete WatchFolderService::instance();
	delete Scheduler::instance();
#ifdef WITH_GUI
	delete g_wndMain;
#endif
	
	g_qmgr->exit();
	Queue::stopQueues();
	Queue::saveQueues();
	Queue::unloadQueues();
	
	runEngines(false);


#ifdef WITH_PYPLUGINS
	if (PyVM::available())
		PluginRegistry::globalExit();

	delete g_pyvm;
	g_pyvm = 0;
#endif
	
	delete g_qmgr;
	exitSettings();
	delete app;
	removePidFile();
	
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
		{
			if (i+1 >= argc)
			{
				std::cerr << argv[i] << " requires a file name\n";
				exit(1);
			}
			m_strPidFile = argv[++i];
		}
		else if (!strcasecmp(argv[i], "--user") || !strcasecmp(argv[i], "-u"))
		{
			if (i+1 >= argc)
			{
				std::cerr << argv[i] << " requires a user name\n";
				exit(1);
			}
			m_strSetUser = argv[++i];
		}
		else if(!strcasecmp(argv[i], "--no-python"))
		{
			qDebug() << "Disabling Python support";
			m_bDisablePython = true;
		}
		else if (!strcasecmp(argv[i], "-c") || !strcasecmp(argv[i], "--config"))
		{
			if (i+1 >= argc)
			{
				std::cerr << argv[i] << " requires a file name\n";
				exit(1);
			}
			m_strSettingsPath = argv[++i];
		}
		else if (!strcasecmp(argv[i], "--rest-socket"))
		{
			if (i+1 >= argc)
			{
				std::cerr << "--rest-socket requires a path\n";
				exit(1);
			}
#ifdef WITH_WEBINTERFACE
			HttpService::setRestSocketPath(argv[++i]);
#else
			std::cerr << "--rest-socket is not available, FatRat was built without the web interface\n";
			exit(1);
#endif
		}
		else if (!strcasecmp(argv[i], "--syslog"))
			Logger::global()->toggleSysLog(true);
		else if(argv[i][0] == '-' && argv[i][1] != 0)
		{
			// Unknown option: none of the options we know has an optional value,
			// so do not swallow the argument that follows (it may be a link).
			std::cerr << "Ignoring unknown option " << argv[i] << "\n";
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

#ifdef WITH_DBUS
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
			const QString msg = QObject::tr("There is already a running instance.\n"
					"If you want to start FatRat anyway, pass --force among arguments.");
#ifdef WITH_GUI
			if (programHasGUI())
				QMessageBox::critical(0, "FatRat", msg);
			else
#endif
				std::cerr << msg.toStdString() << std::endl;
		}
		
		exit(0);
	}
}
#endif

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

#ifdef WITH_GUI
QWidget* getMainWindow()
{
	return g_wndMain;
}
#endif

QString formatSize(qulonglong size, bool persec)
{
	QString rval;
	
	if(size < 1024)
		rval = QString("%L1 B").arg(size);
	else if(size < 1024*1024)
		rval = QString("%L1 KiB").arg(size/1024);
	else if(size < 1024*1024*1024)
		rval = QString("%L1 MiB").arg(double(size)/1024.0/1024.0, 0, 'f', 1);
	else
		rval = QString("%L1 GiB").arg(double(size)/1024.0/1024.0/1024.0, 0, 'f', 1);
	
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
	// PLUGIN_PATH first, so a developer's copy of a plugin overrides the installed one.
	if(const char* p = getenv("PLUGIN_PATH"))
		loadPlugins(p);
	loadPlugins(PLUGIN_LOCATION);
}

void loadPlugins(const char* p)
{
	QDir dir(p);
	QStringList plugins = dir.entryList(QStringList("*.so"));

	// File name -> directory it was tried from. The same plugin twice would
	// register its actions twice.
	static QMap<QString, QString> attempted;

	foreach(QString pl, plugins)
	{
		if (attempted.contains(pl))
		{
			Logger::global()->enterLogMessage(QObject::tr("Skipped a plugin: %1: already tried from %2").arg(pl, attempted[pl]));
			continue;
		}
		attempted[pl] = dir.path();

		PluginLoader::Result r = PluginLoader::load(dir.filePath(pl), programHasGUI());

		switch (r.outcome)
		{
		case PluginLoader::Loaded:
		case PluginLoader::LoadedLegacy:
			Logger::global()->enterLogMessage(QObject::tr("Loaded a plugin:") + ' ' + pl);
			break;
		case PluginLoader::SkippedNeedsGui:
			Logger::global()->enterLogMessage(QObject::tr("Skipped a plugin: %1: %2").arg(pl, r.message));
			break;
		case PluginLoader::Incompatible:
			Logger::global()->enterLogMessage(QObject::tr("WARNING: the plugin is incompatible: %1: %2").arg(pl, r.message));
			break;
		case PluginLoader::Failed:
			Logger::global()->enterLogMessage(QObject::tr("Failed to load a plugin: %1: %2").arg(pl, r.message));
			break;
		}

		if (r.info)
		{
			g_plugins[pl] = *r.info;

			// Plugins written for the plugin API are gated by its version instead.
			if (r.outcome == PluginLoader::LoadedLegacy && strcmp(r.info->version, VERSION))
			{
				qDebug() << "WARNING! Version conflict." << pl << "is" << r.info->version << "but FatRat is" << VERSION;
				Logger::global()->enterLogMessage(QObject::tr("WARNING: the plugin is incompatible:") + ' ' + pl);
			}
		}
	}
}

bool programHasGUI()
{
#ifdef WITH_GUI
	return m_bStartGUI;
#else
	return false;
#endif
}

void showHelp()
{
	std::cout << "FatRat download manager (" VERSION ")\n\n"
			"Copyright (C) 2006-2026 Lubos Dolezel\n"
			"Licensed under the terms of the GNU GPL version 3 as published by the Free Software Foundation\n\n"
			"-f, --force      \tRun the program even if an instance already exists\n"
#ifdef WITH_GUI
			"-i, --hidden     \tHide the GUI at startup (only if the tray icon exists)\n"
			"-n, --nogui      \tStart with no GUI at all\n"
#endif
			"-d, --daemon     \tDaemonize the application (assumes --nogui)\n"
			"-c, --config file\tUse file as settings storage\n"
#ifdef WITH_WEBINTERFACE
			"--rest-socket path\tAlso serve the REST API on a unix socket (no authentication, mode 0600)\n"
#endif
			"--syslog         \tPrint global log contents to syslog\n"
			"-p, --pidfile file\tSave PID to file\n"
			"-u, --user user[:grp]\tSetuid to user, setgid to grp\n"
#ifdef WITH_PYPLUGINS
			"--no-python      \tDisable support for Python extensions\n"
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


void daemonize()
{
	daemon(true, false);
}

#ifdef WITH_DBUS
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
#endif

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
	file.write("\n");
}

void removePidFile()
{
	if (m_strPidFile.isEmpty())
		return;
	if (!QFile::remove(m_strPidFile))
	{
		// Typically the directory (/run) is no longer writable after dropping privileges,
		// but the file was chowned to us: leave it empty rather than hold a stale PID.
		QFile file(m_strPidFile);
		if (file.exists() && file.open(QIODevice::WriteOnly | QIODevice::Truncate))
			file.close();
	}
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

	struct passwd* u = getpwnam(user.constData());
	if (!u)
	{
		Logger::global()->enterLogMessage("Cannot getpwnam() - invalid user name?");
		std::cerr << "Cannot getpwnam() - invalid user name?\n";
		exit(1);
	}

	// Copy what we need, the static buffers get overwritten by later getgr*/getpw* calls
	const uid_t uid = u->pw_uid;
	gid_t gid = u->pw_gid;
	const QByteArray home = u->pw_dir;

	if (!group.isEmpty())
	{
		struct group* g = getgrnam(group.constData());
		if (!g)
		{
			Logger::global()->enterLogMessage("Cannot getgrnam() - invalid group name?");
			std::cerr << "Cannot getgrnam() - invalid group name?\n";
			exit(1);
		}
		gid = g->gr_gid;
	}

	// The pid file was created as the original user: let the new user truncate/remove it
	if (!m_strPidFile.isEmpty() && chown(m_strPidFile.toLocal8Bit().constData(), uid, gid) == -1)
		qDebug() << "Cannot chown the pid file:" << strerror(errno);

	// Order matters: supplementary groups and gid need root, so they go before setuid()
	// (not root: setgroups() would fail, and setgid()/setuid() succeed only for our own ids anyway)
	if (geteuid() == 0 && initgroups(user.constData(), gid) == -1)
	{
		Logger::global()->enterLogMessage("Cannot initgroups()");
		std::cerr << "Cannot initgroups(): " << strerror(errno) << "\n";
		exit(1);
	}
	if (setgid(gid) == -1)
	{
		Logger::global()->enterLogMessage("Cannot setgid()");
		std::cerr << "Cannot setgid()\n";
		exit(1);
	}
	if (setuid(uid) == -1)
	{
		Logger::global()->enterLogMessage("Cannot setuid()");
		std::cerr << "Cannot setuid()\n";
		exit(1);
	}

	setenv("HOME", home.constData(), true);
	if (chdir(home.constData()) == -1)
		qDebug() << "chdir() failed:" << strerror(errno);
}
