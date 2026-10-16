/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2011 Lubos Dolezel <lubos a dolezel.info>

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


#ifndef RSSFEEDPARSER_H
#define RSSFEEDPARSER_H
#include <QIODevice>
#include <QList>
#include <QString>
#include <QXmlStreamReader>

struct RssItem
{
	QString title, descr, url;
	QString source; // RSS URL
	QString id; // TV show episode ID
	enum Next { None, Title, Descr, Url };
};

// Real-world feeds contain HTML entities such as &nbsp; that XML does not define
class RssEntityResolver : public QXmlStreamEntityResolver
{
public:
	QString resolveUndeclaredEntity(const QString&) override { return QString(""); }
};

// Appends the <item> elements of an RSS feed to items. Items read before a
// parse error are kept, and false is returned.
inline bool parseRssFeed(QIODevice* device, const QString& source, QList<RssItem>& items, QString* error = nullptr)
{
	QXmlStreamReader xml(device);
	RssEntityResolver resolver;
	xml.setEntityResolver(&resolver);
	bool inItem = false;
	RssItem item;
	RssItem::Next next = RssItem::None;

	while(!xml.atEnd())
	{
		switch(xml.readNext())
		{
		case QXmlStreamReader::StartElement:
		{
			const auto name = xml.name();

			if(name == QLatin1String("item"))
			{
				inItem = true;
				item.source = source;
			}
			else if(inItem)
			{
				if(name == QLatin1String("title"))
					next = RssItem::Title;
				else if(name == QLatin1String("description"))
					next = RssItem::Descr;
				else if(name == QLatin1String("link") && item.url.isEmpty())
					next = RssItem::Url;
				else if(name == QLatin1String("enclosure"))
				{
					item.url = xml.attributes().value(QLatin1String("url")).toString();
					next = RssItem::None;
				}
				else
					next = RssItem::None;
			}
			else
				next = RssItem::None;
			break;
		}
		case QXmlStreamReader::EndElement:
			if(xml.name() == QLatin1String("item"))
			{
				items << item;
				item = RssItem();
				inItem = false;
			}
			next = RssItem::None;
			break;
		case QXmlStreamReader::Characters:
			if(inItem)
			{
				if(next == RssItem::Descr)
					item.descr += xml.text().toString();
				else if(next == RssItem::Title)
					item.title += xml.text().toString();
				else if(next == RssItem::Url)
					item.url += xml.text().toString();
			}
			break;
		default:
			break;
		}
	}

	if(xml.hasError())
	{
		if(error)
			*error = xml.errorString();
		return false;
	}
	return true;
}

#endif
