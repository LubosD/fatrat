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

#include "SchedulerJson.h"

static const char* const ActionNames[] = { "resume", "stop", "speedLimit" };
static const char* const TimeFormat = "HH:mm";
static const char* const DateTimeFormat = "yyyy-MM-dd'T'HH:mm";

static const QStringList Properties = { "name", "queue", "action", "speedLimit", "repeated", "time", "days", "at", "done" };

static RestError invalid(const QString& message)
{
	return RestError(422, "invalid_property", message);
}

static bool speedLimitOf(const ScheduledAction& a, int& down, int& up)
{
	const QList<QVariant> sp = a.actionArgument.toList();
	if (sp.size() != 2)
		return false;
	down = sp[0].toInt();
	up = sp[1].toInt();
	return true;
}

QJsonObject SchedulerJson::actionToJson(const ScheduledAction& a)
{
	QJsonObject o;
	QJsonArray days;
	int down, up;

	for (bool d : a.daysRepeated)
		days << d;

	o["id"] = a.id;
	o["name"] = a.name;
	o["queue"] = a.queue.toString();
	o["action"] = ActionNames[int(a.action)];
	if (a.action == ScheduledAction::ActionSetSpeedLimit && speedLimitOf(a, down, up))
		o["speedLimit"] = RestApi::speedPairToJson(down, up);
	o["repeated"] = a.repeated;
	o["time"] = a.whenRepeated.isValid() ? a.whenRepeated.toString(TimeFormat) : QString();
	o["days"] = days;
	o["at"] = a.whenOneTime.isValid() ? a.whenOneTime.toString(DateTimeFormat) : QString();
	o["done"] = a.done;
	return o;
}

static ScheduledAction::ActionType actionFromJson(const QJsonValue& v)
{
	const QString s = RestApi::asString(v, "action");
	for (int i = 0; i < 3; i++)
	{
		if (s == ActionNames[i])
			return ScheduledAction::ActionType(i);
	}
	throw invalid("Property \"action\" must be one of resume, stop, speedLimit");
}

static QTime timeFromJson(const QJsonValue& v)
{
	const QString s = RestApi::asString(v, "time");
	const QTime t = QTime::fromString(s, TimeFormat);
	if (s.size() != 5 || !t.isValid())
		throw invalid("Property \"time\" must be a time of day in the form HH:mm");
	return t;
}

static QDateTime dateTimeFromJson(const QJsonValue& v)
{
	const QString s = RestApi::asString(v, "at");
	const QDateTime t = QDateTime::fromString(s, DateTimeFormat);
	if (s.size() != 16 || !t.isValid())
		throw invalid("Property \"at\" must be a local date and time in the form yyyy-MM-ddTHH:mm");
	return t;
}

static void daysFromJson(const QJsonValue& v, bool (&days)[7])
{
	if (!v.isArray() || v.toArray().size() != 7)
		throw invalid("Property \"days\" must be an array of 7 booleans, starting with Monday");

	const QJsonArray a = v.toArray();
	for (int i = 0; i < 7; i++)
		days[i] = RestApi::asBool(a[i], "days");
}

void SchedulerJson::applyAction(ScheduledAction& a, const QJsonObject& body, const QueueLookup& queues, bool create)
{
	ScheduledAction n = a;

	for (const QString& key : body.keys())
	{
		if (!Properties.contains(key))
			throw invalid(QString("Unknown property \"%1\"").arg(key));
	}

	if (create)
	{
		for (const char* required : { "name", "queue", "action" })
		{
			if (!body.contains(required))
				throw invalid(QString("Property \"%1\" is required").arg(required));
		}
	}

	if (body.contains("name"))
		n.name = RestApi::asString(body["name"], "name").trimmed();
	if (n.name.isEmpty())
		throw invalid("Property \"name\" must not be empty");

	if (body.contains("queue"))
	{
		const QString uuid = RestApi::asString(body["queue"], "queue");
		if (!queues(uuid))
			throw invalid("Property \"queue\" does not name an existing queue");
		n.queue = QUuid(uuid);
	}

	if (body.contains("action"))
		n.action = actionFromJson(body["action"]);

	if (n.action == ScheduledAction::ActionSetSpeedLimit)
	{
		int down, up;

		if (body.contains("speedLimit"))
		{
			RestApi::asSpeedPair(body["speedLimit"], "speedLimit", down, up);
			if (down < 0 || up < 0)
				throw invalid("Property \"speedLimit\" must not be negative");
			n.actionArgument = QList<QVariant>() << down << up;
		}
		else if (!speedLimitOf(n, down, up))
			throw invalid("Property \"speedLimit\" is required for the speedLimit action");
	}
	else
		n.actionArgument = QVariant();

	if (body.contains("repeated"))
		n.repeated = RestApi::asBool(body["repeated"], "repeated");

	if (body.contains("time"))
		n.whenRepeated = timeFromJson(body["time"]);
	if (body.contains("days"))
		daysFromJson(body["days"], n.daysRepeated);
	if (body.contains("at"))
		n.whenOneTime = dateTimeFromJson(body["at"]);

	// Changing when an action runs arms it again, unless the client says otherwise
	if (body.contains("repeated") || body.contains("at"))
		n.done = false;
	if (body.contains("done"))
		n.done = RestApi::asBool(body["done"], "done");

	const bool schedule = create || body.contains("repeated") || body.contains("time")
		|| body.contains("days") || body.contains("at");

	if (!schedule)
		;
	else if (n.repeated)
	{
		if (create && !body.contains("time"))
			throw invalid("Property \"time\" is required for a repeated action");

		bool any = false;
		for (bool d : n.daysRepeated)
			any |= d;
		if (!any)
			throw invalid("Property \"days\" must select at least one day");
	}
	else if (!n.whenOneTime.isValid() || (create && !body.contains("at")))
		throw invalid("Property \"at\" is required for a one-time action");

	a = n;
}
