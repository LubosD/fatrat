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
