#pragma once
#include "App.h"

namespace tui
{
	// Ready-made dialogs for the actions of the main views
	namespace dialogs
	{
		void newTransfer(App& app);
		void editTransfer(App& app, const TransferInfo& t);
		void limitTransfers(App& app, const QStringList& uuids, const SpeedPair& initial);
		void globalLimits(App& app);
		void queueEditor(App& app, const QueueInfo* existing);
		void deleteQueue(App& app, const QueueInfo& q);
		void deleteTransfers(App& app, const QStringList& uuids, const QStringList& names);
		void sendToQueue(App& app, const QStringList& uuids, int x, int y);
		void browseDirectory(App& app, const QString& start, const QString& queue, std::function<void(const QString&)> chosen);
		void prompt(App& app, const QString& title, const QString& label, const QString& initial, const QString& okLabel, std::function<void(const QString&)> done);

		qint64 parseKb(const QString& s, bool* ok);       // "KB/s" text to bytes per second
		QString kbText(qint64 bytesPerSecond);
		QString expandPath(const QString& p);
	}
}
