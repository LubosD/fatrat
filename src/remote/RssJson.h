/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2026 Lubos Dolezel <lubos a dolezel.info>

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


#ifndef RSSJSON_H
#define RSSJSON_H
#include "rss/RssFetcher.h"
#include "RestApi.h"
#include <QJsonObject>
#include <QList>
#include <functional>

// JSON mapping and validation for the /api/v1/rss resources. Pure functions: no settings, no queues.
// Every failure is thrown as a RestError.
namespace RssJson
{
	// Finds a queue by UUID. Returns false if there is none, otherwise stores its default directory.
	typedef std::function<bool(const QString& uuid, QString* defaultDirectory)> QueueLookup;

	QString tvsToString(RssRegexp::TVSType tvs);

	QJsonObject feedToJson(const RssFeed& feed);
	QJsonObject regexpToJson(const RssRegexp& re, const QList<RssFeed>& feeds);

	// create: name and url are required; otherwise only the given properties change
	void applyFeed(RssFeed& feed, const QJsonObject& body, bool create);
	// create: feedId and queue are required. A new regexp's target defaults to the queue's default directory.
	void applyRegexp(RssRegexp& re, const QJsonObject& body, const QList<RssFeed>& feeds, const QueueLookup& queues, bool create);

	// {"expression", "text", "tvs"?, "includeRepacks"?, "includeTrailers"?} -> {"matches", "episode"?}
	QJsonObject testRegexp(const QJsonObject& body);

	QStringList episodesFromJson(const QJsonValue& v, RssRegexp::TVSType tvs);
}

#endif
