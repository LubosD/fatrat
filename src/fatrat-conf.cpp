#include <QCoreApplication>
#include <QSettings>
#include <QTextCodec>
#include <iostream>
#include <cstring>
#include "config.h"

using namespace std;

void printUsage();

int main(int argc, char** argv)
{
	QCoreApplication::setOrganizationName("Dolezel");
	QCoreApplication::setOrganizationDomain("dolezel.info");
	QCoreApplication::setApplicationName("fatrat");
	QCoreApplication app(argc, argv);
	QSettings settings;

	QSettings defSettings(DATA_LOCATION "/data/defaults.conf", QSettings::IniFormat, qApp);
	
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
			QString arg(argv[2]);
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
                "key\t\tPrint the key value\n\n";
}

