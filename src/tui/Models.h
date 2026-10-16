#pragma once
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QVector>

namespace tui
{
	struct SpeedPair
	{
		qint64 down = 0, up = 0;
		static SpeedPair fromJson(const QJsonValue& v);
		QJsonObject toJson() const;
	};

	struct QueueInfo
	{
		QString uuid, name, defaultDirectory, moveDirectory;
		bool upAsDown = false;
		SpeedPair transferLimits, speedLimits;
		static QueueInfo fromJson(const QJsonObject& o);
	};

	struct TransferInfo
	{
		enum State { Waiting, Active, Paused, ForcedActive, Failed, Completed, Ready, Testing, Unknown };

		QString uuid, name, cls, message, dataPath, comment, object;
		State state = Unknown;
		bool upload = false;
		qint64 total = 0, done = 0;
		double timeRunning = 0;
		SpeedPair speeds, userSpeedLimits;

		static TransferInfo fromJson(const QJsonObject& o);
		static State parseState(const QString& s);
		static QString stateName(State s);
		bool isActive() const { return state == Active || state == ForcedActive; }
		int percent() const;
	};

	struct Stats
	{
		SpeedPair total;
		QMap<QString, SpeedPair> queues;
		static Stats fromJson(const QJsonObject& o);
	};
}
