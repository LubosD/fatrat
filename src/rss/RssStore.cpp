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

#include "RssFetcher.h"
#include "util/LegacyRegex.h"
#include <QSettings>
#include <QUuid>

extern QSettings* g_settings;

QString RssFetcher::newId()
{
	return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void RssFetcher::loadFeeds(QList<RssFeed>& items)
{
	bool bMissingId = false;
	int amount = g_settings->beginReadArray("rss/feeds");
	
	for(int i=0;i<amount;i++)
	{
		g_settings->setArrayIndex(i);
		RssFeed f;
		f.id = g_settings->value("id").toString();
		f.name = g_settings->value("name").toString();
		f.url = g_settings->value("url").toString();
		f.reply = 0;
		
		if(f.id.isEmpty())
		{
			f.id = newId();
			bMissingId = true;
		}
		items << f;
	}
	
	g_settings->endArray();
	
	if(bMissingId)
		saveFeeds(items);
}

void RssFetcher::loadRegexpsUnfiltered(QList<RssRegexp>& items)
{
	bool bMissingId = false;
	int count = g_settings->beginReadArray("rss/regexps");
	
	for(int i=0;i<count;i++)
	{
		RssRegexp item;
		g_settings->setArrayIndex(i);
		
		item.id = g_settings->value("id").toString();
		if(item.id.isEmpty())
		{
			item.id = newId();
			bMissingId = true;
		}
		item.regexp = legacyRegex(g_settings->value("regexp").toString(), QRegularExpression::CaseInsensitiveOption);
		item.queueUUID = g_settings->value("queueUUID").toString();
		item.source = g_settings->value("source").toString();
		item.target = g_settings->value("target").toString();
		item.tvs = RssRegexp::TVSType( g_settings->value("tvs").toInt() );
		item.epDone = g_settings->value("epdone").toStringList();
		item.from = g_settings->value("epfrom").toString();
		item.to = g_settings->value("epto").toString();
		item.includeTrailers = g_settings->value("includeTrailers").toBool();
		item.includeRepacks = g_settings->value("includeRepacks").toBool();
		item.excludeManuals = g_settings->value("excludeManuals").toBool();
		item.queueIndex = -1;
		item.addPaused = g_settings->value("addPaused").toBool();
		item.linkRegexp = legacyRegex(g_settings->value("linkRegexp").toString(), QRegularExpression::CaseInsensitiveOption);
		
		items << item;
	}
	
	g_settings->endArray();
	
	if(bMissingId)
		saveRegexps(items);
}

void RssFetcher::saveRegexps(const QList<RssRegexp>& items)
{
	g_settings->remove("rss/regexps");
	g_settings->beginWriteArray("rss/regexps", items.size());
	
	for(int i=0;i<items.size();i++)
	{
		g_settings->setArrayIndex(i);
		g_settings->setValue("id", items[i].id.isEmpty() ? newId() : items[i].id);
		g_settings->setValue("regexp", items[i].regexp.pattern());
		g_settings->setValue("queueUUID", items[i].queueUUID);
		g_settings->setValue("source", items[i].source);
		g_settings->setValue("target", items[i].target);
		g_settings->setValue("tvs", items[i].tvs);
		g_settings->setValue("epdone", items[i].epDone);
		g_settings->setValue("epfrom", items[i].from);
		g_settings->setValue("epto", items[i].to);
		g_settings->setValue("includeTrailers", items[i].includeTrailers);
		g_settings->setValue("includeRepacks", items[i].includeRepacks);
		g_settings->setValue("excludeManuals", items[i].excludeManuals);
		g_settings->setValue("addPaused", items[i].addPaused);
		g_settings->setValue("linkRegexp", items[i].linkRegexp.pattern());
	}
	
	g_settings->endArray();
}

void RssFetcher::saveFeeds(const QList<RssFeed>& items)
{
	g_settings->remove("rss/feeds");
	g_settings->beginWriteArray("rss/feeds", items.size());
	
	for(int i=0;i<items.size();i++)
	{
		g_settings->setArrayIndex(i);
		g_settings->setValue("id", items[i].id.isEmpty() ? newId() : items[i].id);
		g_settings->setValue("name", items[i].name);
		g_settings->setValue("url", items[i].url);
	}
	
	g_settings->endArray();
}

