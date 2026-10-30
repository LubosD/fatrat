#include "Models.h"
#include <QJsonValue>
#include <QMap>

namespace tui
{

SpeedPair SpeedPair::fromJson(const QJsonValue& v)
{
	SpeedPair p;
	QJsonObject o = v.toObject();
	p.down = qint64(o["down"].toDouble());
	p.up = qint64(o["up"].toDouble());
	return p;
}

QJsonObject SpeedPair::toJson() const
{
	return QJsonObject{ { "down", double(down) }, { "up", double(up) } };
}

QueueInfo QueueInfo::fromJson(const QJsonObject& o)
{
	QueueInfo q;
	q.uuid = o["uuid"].toString();
	q.name = o["name"].toString();
	q.defaultDirectory = o["defaultDirectory"].toString();
	q.moveDirectory = o["moveDirectory"].toString();
	q.watchDirectory = o["watchDirectory"].toString();
	q.watchStatus = o["watchStatus"].toString();
	q.upAsDown = o["upAsDown"].toBool();
	q.transferLimits = SpeedPair::fromJson(o["transferLimits"]);
	q.speedLimits = SpeedPair::fromJson(o["speedLimits"]);
	return q;
}

TransferInfo::State TransferInfo::parseState(const QString& s)
{
	static const QMap<QString, State> map = {
		{ "Waiting", Waiting }, { "Active", Active }, { "Paused", Paused }, { "ForcedActive", ForcedActive },
		{ "Failed", Failed }, { "Completed", Completed }, { "Ready", Ready }, { "Testing", Testing },
	};
	return map.value(s, Unknown);
}

QString TransferInfo::stateName(State s)
{
	switch (s)
	{
		case Waiting: return "Waiting";
		case Active: return "Active";
		case Paused: return "Paused";
		case ForcedActive: return "Forced";
		case Failed: return "Failed";
		case Completed: return "Completed";
		case Ready: return "Ready";
		case Testing: return "Testing";
		default: return "?";
	}
}

TransferInfo TransferInfo::fromJson(const QJsonObject& o)
{
	TransferInfo t;
	t.uuid = o["uuid"].toString();
	t.name = o["name"].toString();
	t.cls = o["class"].toString();
	t.message = o["message"].toString();
	t.dataPath = o["dataPath"].toString();
	t.comment = o["comment"].toString();
	t.object = o["object"].toString();
	t.completionCommand = o["completionCommand"].toString();
	t.state = parseState(o["state"].toString());
	t.upload = o["mode"].toString() == "Upload";
	t.primaryUpload = o["primaryMode"].toString() == "Upload";
	t.total = qint64(o["total"].toDouble());
	t.done = qint64(o["done"].toDouble());
	t.timeRunning = o["timeRunning"].toDouble();
	t.speeds = SpeedPair::fromJson(o["speeds"]);
	t.userSpeedLimits = SpeedPair::fromJson(o["userSpeedLimits"]);
	return t;
}

int TransferInfo::percent() const
{
	if (total <= 0)
		return state == Completed ? 100 : 0;
	return int(qBound<qint64>(0, done * 100 / total, qint64(100)));
}

Stats Stats::fromJson(const QJsonObject& o)
{
	Stats s;
	s.total = SpeedPair::fromJson(o["total"]);
	QJsonObject q = o["queues"].toObject();
	for (auto it = q.begin(); it != q.end(); ++it)
		s.queues[it.key()] = SpeedPair::fromJson(it.value());
	return s;
}

}
