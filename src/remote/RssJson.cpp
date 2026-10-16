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


#include "RssJson.h"
#include "util/LegacyRegex.h"
#include <QUrl>

static const char* const TvsNames[] = { "none", "season", "episode", "date" };

QString RssJson::tvsToString(RssRegexp::TVSType tvs)
{
	return TvsNames[int(tvs)];
}

static RssRegexp::TVSType tvsFromJson(const QJsonValue& v)
{
	const QString s = RestApi::asString(v, "tvs");
	for (int i = 0; i < 4; i++)
	{
		if (s == TvsNames[i])
			return RssRegexp::TVSType(i);
	}
	throw RestError(422, "invalid_property", "Property \"tvs\" must be one of none, season, episode, date");
}

static RestError invalid(const QString& message)
{
	return RestError(422, "invalid_property", message);
}

static QRegularExpression compile(const QJsonValue& v, const QString& field)
{
	QRegularExpression re = legacyRegex(RestApi::asString(v, field), QRegularExpression::CaseInsensitiveOption);
	if (!re.isValid())
		throw invalid(QString("Property \"%1\" is not a valid regular expression: %2").arg(field, re.errorString()));
	return re;
}

static void defaultRange(RssRegexp::TVSType tvs, QString& from, QString& to)
{
	switch (tvs)
	{
	case RssRegexp::SeasonBased: from = "S00E00"; to = "S99E99"; break;
	case RssRegexp::EpisodeBased: from = "0000"; to = "9999"; break;
	case RssRegexp::DateBased: from = "0000-00-00"; to = "9999-99-99"; break;
	default: from.clear(); to.clear(); break;
	}
}

static void checkRangeBound(RssRegexp::TVSType tvs, const QString& value, const QString& field)
{
	if (!RssFetcher::isValidEpisodeId(tvs, value) || value.contains('|'))
		throw invalid(QString("Property \"%1\" does not match the episode numbering of the selected mode").arg(field));
}

QJsonObject RssJson::feedToJson(const RssFeed& feed)
{
	QJsonObject o;
	o["id"] = feed.id;
	o["name"] = feed.name;
	o["url"] = feed.url;
	return o;
}

QJsonObject RssJson::regexpToJson(const RssRegexp& re, const QList<RssFeed>& feeds)
{
	QJsonObject o;
	QString feedId;

	for (const RssFeed& f : feeds)
	{
		if (f.url == re.source)
		{
			feedId = f.id;
			break;
		}
	}

	o["id"] = re.id;
	o["feedId"] = feedId;
	o["expression"] = re.regexp.pattern();
	o["queue"] = re.queueUUID;
	o["target"] = re.target;
	o["addPaused"] = re.addPaused;
	o["tvs"] = tvsToString(re.tvs);
	o["from"] = re.from;
	o["to"] = re.to;
	o["includeRepacks"] = re.includeRepacks;
	o["includeTrailers"] = re.includeTrailers;
	o["excludeManuals"] = re.excludeManuals;
	o["linkRegexp"] = re.linkRegexp.pattern();
	return o;
}

void RssJson::applyFeed(RssFeed& feed, const QJsonObject& body, bool create)
{
	QString name = feed.name, url = feed.url;

	if (create)
	{
		for (const char* required : { "name", "url" })
		{
			if (!body.contains(required))
				throw invalid(QString("Property \"%1\" is required").arg(required));
		}
	}
	if (body.contains("name"))
		name = RestApi::asString(body["name"], "name").trimmed();
	if (body.contains("url"))
		url = RestApi::asString(body["url"], "url").trimmed();

	if (name.isEmpty())
		throw invalid("Property \"name\" must not be empty");
	if (url.isEmpty())
		throw invalid("Property \"url\" must not be empty");

	feed.name = name;
	feed.url = url;
}

void RssJson::applyRegexp(RssRegexp& re, const QJsonObject& body, const QList<RssFeed>& feeds, const QueueLookup& queues, bool create)
{
	RssRegexp n = re;
	QString queueDir;
	const bool hadMode = body.contains("tvs");

	if (create)
	{
		for (const char* required : { "feedId", "queue" })
		{
			if (!body.contains(required))
				throw invalid(QString("Property \"%1\" is required").arg(required));
		}
	}

	if (body.contains("feedId"))
	{
		const QString id = RestApi::asString(body["feedId"], "feedId");
		bool found = false;
		for (const RssFeed& f : feeds)
		{
			if (f.id == id)
			{
				n.source = f.url;
				found = true;
				break;
			}
		}
		if (!found)
			throw invalid("Property \"feedId\" does not name an existing feed");
	}

	if (body.contains("queue"))
	{
		n.queueUUID = RestApi::asString(body["queue"], "queue");
		if (!queues(n.queueUUID, &queueDir))
			throw invalid("Property \"queue\" does not name an existing queue");
	}

	if (body.contains("expression"))
		n.regexp = compile(body["expression"], "expression");
	if (body.contains("linkRegexp"))
		n.linkRegexp = compile(body["linkRegexp"], "linkRegexp");
	if (body.contains("target"))
		n.target = RestApi::asString(body["target"], "target");
	else if (create)
		n.target = queueDir;
	if (body.contains("addPaused"))
		n.addPaused = RestApi::asBool(body["addPaused"], "addPaused");
	if (body.contains("includeRepacks"))
		n.includeRepacks = RestApi::asBool(body["includeRepacks"], "includeRepacks");
	if (body.contains("includeTrailers"))
		n.includeTrailers = RestApi::asBool(body["includeTrailers"], "includeTrailers");
	if (body.contains("excludeManuals"))
		n.excludeManuals = RestApi::asBool(body["excludeManuals"], "excludeManuals");

	if (hadMode)
		n.tvs = tvsFromJson(body["tvs"]);
	if (body.contains("from"))
		n.from = RestApi::asString(body["from"], "from");
	if (body.contains("to"))
		n.to = RestApi::asString(body["to"], "to");

	if (n.tvs == RssRegexp::None)
	{
		if (hadMode)
			n.from = n.to = QString();
	}
	else
	{
		if (hadMode && !body.contains("from") && !body.contains("to") && n.tvs != re.tvs)
			defaultRange(n.tvs, n.from, n.to);
		if (n.from.isEmpty() && n.to.isEmpty())
			defaultRange(n.tvs, n.from, n.to);
		checkRangeBound(n.tvs, n.from, "from");
		checkRangeBound(n.tvs, n.to, "to");
	}

	if (n.target.trimmed().isEmpty())
		throw invalid("Property \"target\" must not be empty");

	re = n;
}

QJsonObject RssJson::testRegexp(const QJsonObject& body)
{
	const QRegularExpression expr = compile(body["expression"], "expression");
	const QString text = RestApi::asString(body["text"], "text");
	QJsonObject rv;

	RssRegexp re;
	re.tvs = body.contains("tvs") ? tvsFromJson(body["tvs"]) : RssRegexp::None;
	re.includeRepacks = body.contains("includeRepacks") && RestApi::asBool(body["includeRepacks"], "includeRepacks");
	re.includeTrailers = body.contains("includeTrailers") && RestApi::asBool(body["includeTrailers"], "includeTrailers");

	rv["matches"] = expr.match(text).hasMatch();
	if (re.tvs != RssRegexp::None)
	{
		const QString episode = RssFetcher::generateEpisodeName(re, text);
		if (!episode.isEmpty())
			rv["episode"] = episode;
	}
	return rv;
}

QStringList RssJson::episodesFromJson(const QJsonValue& v, RssRegexp::TVSType tvs)
{
	if (tvs == RssRegexp::None)
		throw RestError(409, "no_episode_tracking", "The regexp does not track episodes");

	QStringList list = RestApi::asStringList(v, "episodes");
	list.removeDuplicates();
	for (const QString& s : list)
	{
		if (!RssFetcher::isValidEpisodeId(tvs, s))
			throw invalid(QString("\"%1\" is not a valid episode for the selected mode").arg(s));
	}
	return list;
}
