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


#ifndef RESTRESOURCES_H
#define RESTRESOURCES_H
#include "config.h"
#include "RestApi.h"
#include <QString>
#include <functional>

#ifndef WITH_WEBINTERFACE
#	error This file is not supposed to be included!
#endif

class Queue;
class Transfer;

// RAII holders for the locks that HttpService::findTransfer() leaves taken. Both throw RestError(404) if
// the UUID is unknown, so a handler can never return or throw with a lock still held.
class QueueGuard
{
public:
	enum Lock { NoQueueLock, ReadLock, WriteLock };

	explicit QueueGuard(const QString& uuid, Lock lock = NoQueueLock);
	~QueueGuard();
	QueueGuard(const QueueGuard&) = delete;
	QueueGuard& operator=(const QueueGuard&) = delete;

	Queue* queue() const { return m_queue; }
private:
	Queue* m_queue = nullptr;
	bool m_locked = false;
};

class TransferGuard
{
public:
	explicit TransferGuard(const QString& uuid, bool write = false);
	~TransferGuard();
	TransferGuard(const TransferGuard&) = delete;
	TransferGuard& operator=(const TransferGuard&) = delete;

	Queue* queue() const { return m_queue; }
	Transfer* transfer() const { return m_transfer; }
	int position() const { return m_position; }
private:
	Queue* m_queue = nullptr;
	Transfer* m_transfer = nullptr;
	int m_position = -1;
};

namespace RestResources
{
	// Runs fn on the main thread and waits for it, rethrowing what it throws. The settings arrays have
	// per-QSettings iteration state and the objects behind them (RssFetcher, Scheduler) live on the main
	// thread, so the HTTP threads do all of their storage access there.
	void onMainThread(const std::function<void()>& fn);
	// Registers the core routes with RestApi. Engine-specific routes are registered by the engines themselves.
	void registerAll();
	// The /rss resources. Called by registerAll().
	void registerRss();
	// The /scheduler resources. Called by registerAll().
	void registerScheduler();
}

#endif
