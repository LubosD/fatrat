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

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

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
