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


#include "RestResources.h"
#include "RssJson.h"
#include "Queue.h"
#include "rss/RssFetcher.h"

using RestResources::onMainThread;

static bool lookupQueue(const QString& uuid, QString* defaultDirectory)
{
	try
	{
		QueueGuard g(uuid, QueueGuard::ReadLock);
		*defaultDirectory = g.queue()->defaultDirectory();
		return true;
	}
	catch (const RestError&)
	{
		return false;
	}
}

static RestError feedNotFound()
{
	return RestError(404, "feed_not_found", "Invalid feed id");
}

static RestError regexpNotFound()
{
	return RestError(404, "regexp_not_found", "Invalid regexp id");
}

static int findFeed(const QList<RssFeed>& feeds, const QString& id)
{
	for (int i = 0; i < feeds.size(); i++)
	{
		if (feeds[i].id == id)
			return i;
	}
	throw feedNotFound();
}

static int findRegexp(const QList<RssRegexp>& list, const QString& id)
{
	for (int i = 0; i < list.size(); i++)
	{
		if (list[i].id == id)
			return i;
	}
	throw regexpNotFound();
}

static void checkUniqueUrl(const QList<RssFeed>& feeds, const RssFeed& feed)
{
	for (const RssFeed& f : feeds)
	{
		if (f.id != feed.id && f.url == feed.url)
			throw RestError(409, "duplicate_feed", "A feed with this URL already exists");
	}
}

static RestResult listFeeds(const RestRequest&)
{
	QJsonArray rv;

	onMainThread([&] {
		QList<RssFeed> feeds;
		RssFetcher::loadFeeds(feeds);
		for (const RssFeed& f : feeds)
			rv << RssJson::feedToJson(f);
	});
	return RestResult::ok(rv);
}

static RestResult createFeed(const RestRequest& req)
{
	const QJsonObject body = req.jsonObject();
	QJsonObject rv;

	onMainThread([&] {
		QList<RssFeed> feeds;
		RssFetcher::loadFeeds(feeds);

		RssFeed feed;
		feed.id = RssFetcher::newId();
		RssJson::applyFeed(feed, body, true);
		checkUniqueUrl(feeds, feed);

		feeds << feed;
		RssFetcher::saveFeeds(feeds);
		rv = RssJson::feedToJson(feed);
	});
	return RestResult::created(rv, QString("%1/rss/feeds/%2").arg(RestApi::Prefix, rv["id"].toString()));
}

static RestResult patchFeed(const RestRequest& req)
{
	const QJsonObject body = req.jsonObject();
	const QString id = req.pathParams["feed"];
	QJsonObject rv;

	onMainThread([&] {
		QList<RssFeed> feeds;
		QList<RssRegexp> regexps;
		RssFetcher::loadFeeds(feeds);
		RssFetcher::loadRegexpsUnfiltered(regexps);

		RssFeed& feed = feeds[findFeed(feeds, id)];
		const QString oldUrl = feed.url;

		RssJson::applyFeed(feed, body, false);
		checkUniqueUrl(feeds, feed);

		if (feed.url != oldUrl)
		{
			for (RssRegexp& re : regexps)
			{
				if (re.source == oldUrl)
					re.source = feed.url;
			}
			RssFetcher::saveRegexps(regexps);
		}

		RssFetcher::saveFeeds(feeds);
		rv = RssJson::feedToJson(feed);
	});
	return RestResult::ok(rv);
}

static RestResult deleteFeed(const RestRequest& req)
{
	const QString id = req.pathParams["feed"];

	onMainThread([&] {
		QList<RssFeed> feeds;
		QList<RssRegexp> regexps;
		RssFetcher::loadFeeds(feeds);
		RssFetcher::loadRegexpsUnfiltered(regexps);

		const int ix = findFeed(feeds, id);
		const QString url = feeds[ix].url;

		for (int i = 0; i < regexps.size(); i++)
		{
			if (regexps[i].source == url)
				regexps.removeAt(i--);
		}
		feeds.removeAt(ix);

		RssFetcher::saveRegexps(regexps);
		RssFetcher::saveFeeds(feeds);
	});
	return RestResult::noContent();
}

static RestResult listRegexps(const RestRequest&)
{
	QJsonArray rv;

	onMainThread([&] {
		QList<RssFeed> feeds;
		QList<RssRegexp> regexps;
		RssFetcher::loadFeeds(feeds);
		RssFetcher::loadRegexpsUnfiltered(regexps);
		for (const RssRegexp& re : regexps)
			rv << RssJson::regexpToJson(re, feeds);
	});
	return RestResult::ok(rv);
}

static RestResult createRegexp(const RestRequest& req)
{
	const QJsonObject body = req.jsonObject();
	QJsonObject rv;

	onMainThread([&] {
		QList<RssFeed> feeds;
		QList<RssRegexp> regexps;
		RssFetcher::loadFeeds(feeds);
		RssFetcher::loadRegexpsUnfiltered(regexps);

		RssRegexp re;
		re.id = RssFetcher::newId();
		re.includeRepacks = true;
		RssJson::applyRegexp(re, body, feeds, lookupQueue, true);

		regexps << re;
		RssFetcher::saveRegexps(regexps);
		rv = RssJson::regexpToJson(re, feeds);
	});
	return RestResult::created(rv, QString("%1/rss/regexps/%2").arg(RestApi::Prefix, rv["id"].toString()));
}

static RestResult patchRegexp(const RestRequest& req)
{
	const QJsonObject body = req.jsonObject();
	const QString id = req.pathParams["regexp"];
	QJsonObject rv;

	onMainThread([&] {
		QList<RssFeed> feeds;
		QList<RssRegexp> regexps;
		RssFetcher::loadFeeds(feeds);
		RssFetcher::loadRegexpsUnfiltered(regexps);

		RssRegexp& re = regexps[findRegexp(regexps, id)];
		RssJson::applyRegexp(re, body, feeds, lookupQueue, false);

		RssFetcher::saveRegexps(regexps);
		rv = RssJson::regexpToJson(re, feeds);
	});
	return RestResult::ok(rv);
}

static RestResult deleteRegexp(const RestRequest& req)
{
	const QString id = req.pathParams["regexp"];

	onMainThread([&] {
		QList<RssRegexp> regexps;
		RssFetcher::loadRegexpsUnfiltered(regexps);
		regexps.removeAt(findRegexp(regexps, id));
		RssFetcher::saveRegexps(regexps);
	});
	return RestResult::noContent();
}

static RestResult getDownloaded(const RestRequest& req)
{
	const QString id = req.pathParams["regexp"];
	QJsonArray rv;

	onMainThread([&] {
		QList<RssRegexp> regexps;
		RssFetcher::loadRegexpsUnfiltered(regexps);
		for (const QString& s : regexps[findRegexp(regexps, id)].epDone)
			rv << s;
	});
	return RestResult::ok(rv);
}

static RestResult putDownloaded(const RestRequest& req)
{
	const QJsonValue body = req.json();
	const QString id = req.pathParams["regexp"];

	onMainThread([&] {
		QList<RssRegexp> regexps;
		RssFetcher::loadRegexpsUnfiltered(regexps);

		RssRegexp& re = regexps[findRegexp(regexps, id)];
		re.epDone = RssJson::episodesFromJson(body, re.tvs);
		RssFetcher::saveRegexps(regexps);
	});
	return RestResult::noContent();
}

static RestResult testRegexp(const RestRequest& req)
{
	return RestResult::ok(RssJson::testRegexp(req.jsonObject()));
}

static RestResult refreshFeeds(const RestRequest&)
{
	QMetaObject::invokeMethod(RssFetcher::instance(), "refresh", Qt::QueuedConnection);
	return RestResult::accepted();
}

void RestResources::registerRss()
{
	RestApi::registerRoute("GET", "/rss/feeds", listFeeds);
	RestApi::registerRoute("POST", "/rss/feeds", createFeed);
	RestApi::registerRoute("PATCH", "/rss/feeds/:feed", patchFeed);
	RestApi::registerRoute("DELETE", "/rss/feeds/:feed", deleteFeed);

	RestApi::registerRoute("GET", "/rss/regexps", listRegexps);
	RestApi::registerRoute("POST", "/rss/regexps", createRegexp);
	RestApi::registerRoute("PATCH", "/rss/regexps/:regexp", patchRegexp);
	RestApi::registerRoute("DELETE", "/rss/regexps/:regexp", deleteRegexp);
	RestApi::registerRoute("GET", "/rss/regexps/:regexp/downloaded", getDownloaded);
	RestApi::registerRoute("PUT", "/rss/regexps/:regexp/downloaded", putDownloaded);

	RestApi::registerRoute("POST", "/rss/test", testRegexp);
	RestApi::registerRoute("POST", "/rss/refresh", refreshFeeds);
}
