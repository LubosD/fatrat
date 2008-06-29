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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
USA.
*/

#include "TorrentIPFilter.h"
#include <QFile>

bool loadIPFilter(QString textFile, libtorrent::ip_filter* filter)
{
	QFile file(textFile);
	
	if(!file.open(QIODevice::ReadOnly))
		return false;
	
	while(file.canReadLine())
	{
		QByteArray line = file.readLine();
		QByteArray from, to;
		
		int colon, dash;
		
		colon = line.indexOf(':') + 1;
		if(!colon)
			continue;
		
		dash = line.indexOf('-', colon+1);
		if(dash < 0)
			continue;
		
		from = line.mid(colon, dash-colon);
		to = line.mid(dash+1);
		
		filter->add_rule(asio::ip::address::from_string(from.data()),
				 asio::ip::address::from_string(to.data()), libtorrent::ip_filter::blocked);
	}
	
	return true;
}
