#include <QCoreApplication>
#include <QSettings>
#include <QStringList>
#include <iostream>
#include <cstring>
#include "config.h"

#ifdef WITH_PYPLUGINS
#	include "python/PluginRepository.h"
#endif

using namespace std;

void printUsage();
#ifdef WITH_PYPLUGINS
static int pluginCommand(const QStringList& arguments);
#endif

int main(int argc, char** argv)
{
	QCoreApplication::setOrganizationName("Dolezel");
	QCoreApplication::setOrganizationDomain("dolezel.info");
	QCoreApplication::setApplicationName("fatrat");
	QCoreApplication app(argc, argv);
	QSettings settings;

	QSettings defSettings(DATA_LOCATION "/data/defaults.conf", QSettings::IniFormat, qApp);
	
#ifdef WITH_PYPLUGINS
	if (argc > 1 && !strcmp(argv[1], "plugins"))
	{
		QStringList arguments;

		for (int i = 2; i < argc; i++)
			arguments << QString::fromLocal8Bit(argv[i]);

		return pluginCommand(arguments);
	}
#endif

	if (argc == 1)
	{
		printUsage();
		return 1;
	}
	else if (strcmp(argv[1], "-w") != 0)
	{
		for (int i = 1; i < argc; i++)
		{
			QVariant defaultValue = defSettings.value(argv[i]);
			QVariant value = settings.value(argv[i], defaultValue);

			if (value.isNull())
				cerr << "ERROR: Key " << argv[i] << " not found\n";
			else
				cout << argv[i] << " = " << value.toString().toStdString() << endl;
		}
	}
	else
	{
		for (int i = 2; i < argc; i++)
		{
			// argv[i], not argv[2]: every pair on the command line, not
			// the first one over and over.
			QString arg(argv[i]);
			int pos = arg.indexOf('=');
			if (pos == -1)
			{
				cerr << "WARNING: Skipped invalid argument: " << argv[i] << endl;
				continue;
			}
			
			settings.setValue(arg.left(pos), arg.mid(pos+1));
		}
	}

	return 0;
}

void printUsage()
{
	std::cout << "Usage:\n\n"
                "fatrat-conf -w key=value [key=value...]\n"
                "fatrat-conf key [key...]\n\n"
                "-w key=value\tWrite a settings value 'key' with a 'value'\n"
                "key\t\tPrint the key value\n\n"
#ifdef WITH_PYPLUGINS
                "fatrat-conf plugins <command>\t\tManage Python extensions:\n"
                "  repos\t\t\t\tList the plugin repositories you have added\n"
                "  add <git-url|directory>\tAdd a repository (its plugins run with your privileges)\n"
                "  remove-repo <id>\t\tForget a repository; installed plugins stay\n"
                "  refresh [id]\t\t\tFetch repositories again\n"
                "  available\t\t\tList what the repositories offer\n"
                "  install <repo-id> <plugin> [--with-dependencies]\n"
                "               \t\t\tInstall or update one plugin\n"
                "  uninstall <plugin>\t\tRemove an installed plugin\n"
                "  list\t\t\t\tList installed plugins\n"
                "  updates\t\t\tList plugins with a newer version available\n\n"
#endif
                ;
}

#ifdef WITH_PYPLUGINS
static int pluginCommand(const QStringList& arguments)
{
	QString command = arguments.value(0);
	QString error;

	if (command == "repos")
	{
		foreach (const PluginRepository::Repository& repository, PluginRepository::repositories())
		{
			cout << qPrintable(repository.id) << "\t"
			     << qPrintable(repository.source)
			     << (repository.local ? "\t(local)" : "\t")
			     << (repository.revision.isEmpty() ? "" : "\t")
			     << qPrintable(repository.revision.left(10)) << endl;
		}
		return 0;
	}

	if (command == "list")
	{
		foreach (const PluginRepository::Installed& record, PluginRepository::installed())
		{
			cout << qPrintable(record.plugin) << "\t" << qPrintable(record.version)
			     << "\t" << qPrintable(record.repository) << endl;
		}
		return 0;
	}

	if (command == "available" || command == "updates")
	{
		QList<PluginRepository::Available> entries = (command == "updates")
			? PluginRepository::updates() : PluginRepository::available();

		foreach (const PluginRepository::Available& entry, entries)
		{
			cout << qPrintable(entry.plugin.name) << "\t" << qPrintable(entry.plugin.version)
			     << "\t" << qPrintable(entry.repository)
			     << "\t" << qPrintable(entry.installedVersion.isEmpty()
						   ? QString("-") : entry.installedVersion)
			     << "\t" << qPrintable(entry.plugin.description) << endl;
		}
		return 0;
	}

	if (command == "add" && arguments.size() > 1)
	{
		// Worth saying out loud, because this is the moment of trust.
		cerr << "A plugin from this repository runs with your full user privileges." << endl;

		if (!PluginRepository::addRepository(arguments[1], &error))
		{
			cerr << "ERROR: " << qPrintable(error) << endl;
			return 1;
		}
		return 0;
	}

	if (command == "remove-repo" && arguments.size() > 1)
	{
		if (!PluginRepository::removeRepository(arguments[1], &error))
		{
			cerr << "ERROR: " << qPrintable(error) << endl;
			return 1;
		}
		return 0;
	}

	if (command == "refresh")
	{
		if (!PluginRepository::refresh(arguments.value(1), &error))
		{
			cerr << "ERROR: " << qPrintable(error) << endl;
			return 1;
		}
		return 0;
	}

	if (command == "install" && arguments.size() > 2)
	{
		QStringList dependencies = PluginRepository::dependenciesOf(arguments[1], arguments[2]);
		bool withDependencies = arguments.contains("--with-dependencies");

		if (!dependencies.isEmpty() && !withDependencies)
		{
			// Packages from PyPI are code the user did not choose, so
			// they never come along unasked.
			cerr << qPrintable(arguments[2]) << " needs these packages from PyPI:" << endl;
			foreach (const QString& dependency, dependencies)
				cerr << "  " << qPrintable(dependency) << endl;
			cerr << "Install them too with: fatrat-conf plugins install "
			     << qPrintable(arguments[1]) << " " << qPrintable(arguments[2])
			     << " --with-dependencies" << endl;
			return 1;
		}

		if (!PluginRepository::install(arguments[1], arguments[2], &error, withDependencies))
		{
			cerr << "ERROR: " << qPrintable(error) << endl;
			return 1;
		}
		cout << "Installed. A running FatRat loads it when it next starts, or install it from a front end to load it at once." << endl;
		return 0;
	}

	if (command == "uninstall" && arguments.size() > 1)
	{
		if (!PluginRepository::uninstall(arguments[1], &error))
		{
			cerr << "ERROR: " << qPrintable(error) << endl;
			return 1;
		}
		return 0;
	}

	printUsage();
	return 1;
}
#endif

