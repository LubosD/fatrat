/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

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
		
		filter->add_rule(libtorrent::address::from_string(from.data()),
				 libtorrent::address::from_string(to.data()), libtorrent::ip_filter::blocked);
	}
	
	return true;
}
