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
		QString uuid, name, defaultDirectory, moveDirectory, watchDirectory, watchStatus;
		bool upAsDown = false;
		SpeedPair transferLimits, speedLimits;
		static QueueInfo fromJson(const QJsonObject& o);
		bool watchProblem() const { return watchStatus == "missing" || watchStatus == "not_writable" || watchStatus == "duplicate"; }
	};

	struct TransferInfo
	{
		enum State { Waiting, Active, Paused, ForcedActive, Failed, Completed, Ready, Testing, Unknown };

		QString uuid, name, cls, message, dataPath, comment, object, completionCommand;
		State state = Unknown;
		bool upload = false;          // the current mode: a finished torrent seeds in Upload mode
		bool primaryUpload = false;   // what the transfer is; decides Destination vs Source
		qint64 total = 0, done = 0;
		double timeRunning = 0;
		SpeedPair speeds, userSpeedLimits;

		static TransferInfo fromJson(const QJsonObject& o);
		static State parseState(const QString& s);
		static QString stateName(State s);
		bool isActive() const { return state == Active || state == ForcedActive; }
		// What Resume does, like the desktop: a forced transfer becomes a normal one, a paused or
		// failed one waits for a free slot; running, waiting and finished transfers are left alone.
		bool canResume() const { return state == Paused || state == Failed || state == ForcedActive; }
		State resumedState() const { return state == ForcedActive ? Active : Waiting; }
		int percent() const;
	};

	struct Stats
	{
		SpeedPair total;
		QMap<QString, SpeedPair> queues;
		static Stats fromJson(const QJsonObject& o);
	};
}
