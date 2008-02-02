#include "NetIface.h"
#include <QFile>
#include <QStringList>
#include <QtDebug>

QString getRoutingInterface4()
{
	QFile route ("/proc/net/route");
	if(!route.open(QIODevice::ReadOnly))
	{
		qDebug() << route.error();
		return QString();
	}
	
	while(1)
	{
		QString line = route.readLine();
		QStringList parts = line.split(QRegExp("\\W+"), QString::SkipEmptyParts);
		
		if(line.isEmpty())
			break;
		
		if(parts.size() < 4)
			continue;
		
		if(parts[1] == "00000000")
			return parts[0];
	}
	
	return QString();
}

QPair<qint64, qint64> getInterfaceStats(QString iface)
{
	QFile dev ("/proc/net/dev");
	if(!dev.open(QIODevice::ReadOnly))
		return QPair<qint64, qint64>(-1, -1);
	
	while(1)
	{
		QString line = dev.readLine().replace(':', ' ');
		QStringList parts = line.split(QRegExp("\\W+"), QString::SkipEmptyParts);
		
		if(line.isEmpty())
			break;
		if(parts[0] == iface)
		{
			qint64 down, up;
			down = parts[1].toLongLong();
			up = parts[9].toLongLong();
			
			return QPair<qint64, qint64>(down, up);
		}
	}
	
	return QPair<qint64, qint64>(-1, -1);
}
