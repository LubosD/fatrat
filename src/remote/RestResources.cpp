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
#include "DirectoryListing.h"
#include "HttpService.h"
#include "RuntimeException.h"
#include "Queue.h"
#include "RestTasks.h"
#include "Transfer.h"
#include "TransferAdder.h"
#include "TransferFactory.h"
#include "TransferHttpService.h"
#include "plugins/FormValidator.h"
#include "watch/WatchFolderQueues.h"
#include "Settings.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QThread>
#include <exception>
#include <QReadWriteLock>
#include <QTemporaryFile>
#include <QJsonDocument>
#include <algorithm>

extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;
extern QVector<EngineEntry> g_enginesDownload;
extern QVector<EngineEntry> g_enginesUpload;
extern QVector<SettingsItem> g_settingsPages;

static RestError queueNotFound()
{
	return RestError(404, "queue_not_found", "Invalid queue UUID");
}

static RestError transferNotFound()
{
	return RestError(404, "transfer_not_found", "Invalid transfer UUID");
}

QueueGuard::QueueGuard(const QString& uuid, Lock lock)
{
	g_queuesLock.lockForRead();

	for (Queue* q : g_queues)
	{
		if (q->uuid() == uuid)
		{
			m_queue = q;
			break;
		}
	}

	if (!m_queue)
	{
		g_queuesLock.unlock();
		throw queueNotFound();
	}

	if (lock == ReadLock)
		m_queue->lock();
	else if (lock == WriteLock)
		m_queue->lockW();
	m_locked = (lock != NoQueueLock);
}

QueueGuard::~QueueGuard()
{
	if (m_locked)
		m_queue->unlock();
	g_queuesLock.unlock();
}

TransferGuard::TransferGuard(const QString& uuid, bool write)
{
	// returns with g_queuesLock and the queue's lock held, or with neither if there is no such transfer
	m_position = HttpService::findTransfer(uuid, &m_queue, &m_transfer, write);
	if (!m_transfer)
		throw transferNotFound();
}

TransferGuard::~TransferGuard()
{
	m_queue->unlock();
	g_queuesLock.unlock();
}

// ---- JSON mapping ----

static QJsonObject transferToJson(Transfer* t)
{
	QJsonObject o;
	int down, up;

	o["name"] = t->name();
	o["state"] = Transfer::state2string(t->state());
	o["class"] = t->myClass();
	o["message"] = t->message();
	o["mode"] = (t->mode() == Transfer::Download) ? "Download" : "Upload";
	o["primaryMode"] = (t->primaryMode() == Transfer::Download) ? "Download" : "Upload";
	o["dataPath"] = t->dataPath();
	o["dataPathIsDir"] = QFileInfo(t->dataPath()).isDir();
	o["total"] = double(t->total());
	o["done"] = double(t->done());
	o["uuid"] = t->uuid();
	o["comment"] = t->comment();
	o["completionCommand"] = t->autoActionCommand(Transfer::Completed);
	o["object"] = t->object();
	o["timeRunning"] = double(t->timeRunning());

	t->speeds(down, up);
	o["speeds"] = RestApi::speedPairToJson(down, up);

	t->userSpeedLimits(down, up);
	o["userSpeedLimits"] = RestApi::speedPairToJson(down, up);

	if (TransferHttpService* srv = dynamic_cast<TransferHttpService*>(t))
		o["detailsScript"] = QString::fromUtf8(srv->detailsScript());

	return o;
}

// the caller holds g_queuesLock
static QJsonObject queueToJson(Queue* q)
{
	QJsonObject o;
	int down, up;

	o["name"] = q->name();
	o["uuid"] = q->uuid();
	o["defaultDirectory"] = q->defaultDirectory();
	o["moveDirectory"] = q->moveDirectory();
	o["watchDirectory"] = q->watchDirectory();
	o["watchStatus"] = WatchFolderStatus::status(WatchFolderQueues::entries(), q->uuid());
	o["upAsDown"] = q->upAsDown();

	q->transferLimits(down, up);
	o["transferLimits"] = RestApi::speedPairToJson(down, up);

	q->speedLimits(down, up);
	o["speedLimits"] = RestApi::speedPairToJson(down, up);

	return o;
}

static QJsonArray speedDataToJson(const QList<QPair<int,int>>& data)
{
	QJsonArray a;
	for (const QPair<int,int>& el : data)
		a << RestApi::speedPairToJson(el.first, el.second);
	return a;
}

// ---- property updates: validate everything first, apply afterwards ----

static RestError unknownProperty(const QString& name)
{
	return RestError(422, "invalid_property", QString("Unknown property: %1").arg(name));
}

namespace
{
	struct QueueUpdate
	{
		std::optional<QString> name, defaultDirectory, moveDirectory, watchDirectory;
		std::optional<bool> upAsDown;
		std::optional<QPair<int,int>> speedLimits, transferLimits;

		static QueueUpdate parse(const QJsonObject& o)
		{
			QueueUpdate u;
			for (auto it = o.begin(); it != o.end(); ++it)
			{
				const QString key = it.key();
				int down, up;

				if (key == "name")
					u.name = RestApi::asString(it.value(), key);
				else if (key == "defaultDirectory")
					u.defaultDirectory = RestApi::asString(it.value(), key);
				else if (key == "moveDirectory")
					u.moveDirectory = RestApi::asString(it.value(), key);
				else if (key == "watchDirectory")
				{
					u.watchDirectory = RestApi::asString(it.value(), key).trimmed();
					if (!u.watchDirectory->isEmpty() && !QDir::isAbsolutePath(*u.watchDirectory))
						throw RestError(422, "invalid_watch_directory", QString("Not an absolute path: %1").arg(*u.watchDirectory));
				}
				else if (key == "upAsDown")
					u.upAsDown = RestApi::asBool(it.value(), key);
				else if (key == "speedLimits")
				{
					RestApi::asSpeedPair(it.value(), key, down, up);
					u.speedLimits = qMakePair(down, up);
				}
				else if (key == "transferLimits")
				{
					RestApi::asSpeedPair(it.value(), key, down, up);
					u.transferLimits = qMakePair(down, up);
				}
				else if (key != "uuid")
					throw unknownProperty(key);
			}
			return u;
		}

		// Two queues must not watch the same folder. The caller holds g_queuesLock.
		void checkWatchDirectory(const QString& selfUuid) const
		{
			if (!watchDirectory || watchDirectory->isEmpty())
				return;

			const QString owner = WatchFolderStatus::owner(WatchFolderQueues::entries(), *watchDirectory, selfUuid);
			if (owner.isEmpty())
				return;

			QString name = owner;
			for (Queue* q : g_queues)
			{
				if (q->uuid() == owner)
					name = q->name();
			}
			throw RestError(422, "watch_directory_in_use", QString("The queue \"%1\" already watches %2").arg(name, *watchDirectory));
		}

		void apply(Queue* q) const
		{
			if (name) q->setName(*name);
			if (defaultDirectory) q->setDefaultDirectory(*defaultDirectory);
			if (moveDirectory) q->setMoveDirectory(*moveDirectory);
			if (watchDirectory) q->setWatchDirectory(*watchDirectory);
			if (upAsDown) q->setUpAsDown(*upAsDown);
			if (speedLimits) q->setSpeedLimits(speedLimits->first, speedLimits->second);
			if (transferLimits) q->setTransferLimits(transferLimits->first, transferLimits->second);
		}
	};

	struct TransferUpdate
	{
		std::optional<Transfer::State> state;
		std::optional<QString> comment, object, queue, completionCommand;
		std::optional<QPair<int,int>> userSpeedLimits;

		static TransferUpdate parse(const QJsonObject& o)
		{
			TransferUpdate u;
			for (auto it = o.begin(); it != o.end(); ++it)
			{
				const QString key = it.key();

				if (key == "state")
				{
					const QString s = RestApi::asString(it.value(), key);
					static const Transfer::State all[] = { Transfer::Waiting, Transfer::Active, Transfer::ForcedActive,
						Transfer::Paused, Transfer::Failed, Transfer::Completed };
					bool known = false;
					for (Transfer::State st : all)
					{
						if (Transfer::state2string(st) == s)
						{
							u.state = st;
							known = true;
						}
					}
					if (!known)
						throw RestError(422, "invalid_property", QString("Unknown transfer state: %1").arg(s));
				}
				else if (key == "comment")
					u.comment = RestApi::asString(it.value(), key);
				else if (key == "completionCommand")
					u.completionCommand = RestApi::asString(it.value(), key);
				else if (key == "object")
					u.object = RestApi::asString(it.value(), key);
				else if (key == "queue")
					u.queue = RestApi::asString(it.value(), key);
				else if (key == "userSpeedLimits")
				{
					int down, up;
					RestApi::asSpeedPair(it.value(), key, down, up);
					u.userSpeedLimits = qMakePair(down, up);
				}
				else
					throw unknownProperty(key);
			}
			return u;
		}

		// Like the desktop dialog's GenericOptsForm::accept(): a download needs
		// an existing directory, an upload a readable file. Run for every target
		// before any is changed.
		void check(Transfer* t) const
		{
			if (!object)
				return;

			if (!QDir::isAbsolutePath(*object))
				throw RestError(422, "invalid_property", QString("Not an absolute path: %1").arg(*object));

			QFileInfo info(*object);
			bool ok = (t->primaryMode() == Transfer::Download)
				? (info.isDir() && info.isReadable())
				: (info.isFile() && info.isReadable());

			if (!ok)
			{
				throw RestError(422, "invalid_property", (t->primaryMode() == Transfer::Download)
					? QString("Not a readable directory: %1").arg(*object)
					: QString("Not a readable file: %1").arg(*object));
			}
		}

		void apply(Transfer* t) const
		{
			// The only step that can fail on I/O, so it goes first: a 422 then means this transfer was not touched.
			if (object)
			{
				try
				{
					t->setObject(*object);
				}
				catch (const RuntimeException& e)
				{
					throw RestError(422, "invalid_property", e.what());
				}
			}
			if (state) TransferFactory::instance()->setState(t, *state);
			if (comment) t->setComment(*comment);
			if (completionCommand) t->setAutoActionCommand(Transfer::Completed, *completionCommand);
			if (userSpeedLimits) t->setUserSpeedLimits(userSpeedLimits->first, userSpeedLimits->second);
		}
	};

	struct AddOptions
	{
		QStringList urls;
		bool upload = false;
		QString engine;
		QString target;
		bool paused = false;
		int down = 0, up = 0;

		static AddOptions parse(const QJsonObject& o, bool withUrls)
		{
			AddOptions a;
			for (auto it = o.begin(); it != o.end(); ++it)
			{
				const QString key = it.key();
				if (key == "urls" && withUrls)
					a.urls = RestApi::asStringList(it.value(), key);
				else if (key == "upload")
					a.upload = RestApi::asBool(it.value(), key);
				else if (key == "class")
					a.engine = RestApi::asString(it.value(), key);
				else if (key == "target")
					a.target = RestApi::asString(it.value(), key);
				else if (key == "paused")
					a.paused = RestApi::asBool(it.value(), key);
				else if (key == "speedLimits")
					RestApi::asSpeedPair(it.value(), key, a.down, a.up);
				else
					throw unknownProperty(key);
			}

			if (withUrls && !o.contains("urls"))
				throw RestError(422, "invalid_property", "Property \"urls\" is required");
			return a;
		}
	};
}

// ---- handlers ----

static RestResult getTransferClasses(const RestRequest&)
{
	QJsonArray rv;
	auto add = [&rv](const QVector<EngineEntry>& engines, const char* mode) {
		for (const EngineEntry& e : engines)
		{
			if (e.removed)
				continue;
			QJsonObject o;
			o["mode"] = mode;
			o["shortName"] = e.shortName;
			o["longName"] = e.longName;
			rv << o;
		}
	};
	add(g_enginesDownload, "Download");
	add(g_enginesUpload, "Upload");
	return RestResult::ok(rv);
}

static RestResult listQueues(const RestRequest&)
{
	QReadLocker l(&g_queuesLock);
	QJsonArray rv;
	for (Queue* q : g_queues)
		rv << queueToJson(q);
	return RestResult::ok(rv);
}

static RestResult getStats(const RestRequest&)
{
	QReadLocker l(&g_queuesLock);
	QJsonObject perQueue;
	qint64 totalDown = 0, totalUp = 0;

	for (Queue* q : g_queues)
	{
		qint64 qDown = 0, qUp = 0;

		q->lock();
		for (int i = 0; i < q->size(); i++)
		{
			int down, up;
			q->at(i)->speeds(down, up);
			qDown += down;
			qUp += up;
		}
		q->unlock();

		perQueue[q->uuid()] = RestApi::speedPairToJson(int(qDown), int(qUp));
		totalDown += qDown;
		totalUp += qUp;
	}

	QJsonObject rv;
	rv["total"] = RestApi::speedPairToJson(int(totalDown), int(totalUp));
	rv["queues"] = perQueue;
	return RestResult::ok(rv);
}

static RestResult createQueue(const RestRequest& req)
{
	QueueUpdate update = QueueUpdate::parse(req.jsonObject());

	std::unique_ptr<Queue> q(new Queue);
	update.apply(q.get());
	QJsonObject json;

	{
		QWriteLocker l(&g_queuesLock);
		update.checkWatchDirectory(q->uuid());
		Queue* added = q.release();
		g_queues << added;
		json = queueToJson(added);
	}
	WatchFolderService::requestReload();
	return RestResult::created(json, QString("%1/queues/%2").arg(RestApi::Prefix, json["uuid"].toString()));
}

static RestResult patchQueue(const RestRequest& req)
{
	QueueUpdate update = QueueUpdate::parse(req.jsonObject());
	QueueGuard g(req.pathParams["queue"]);

	update.checkWatchDirectory(g.queue()->uuid());
	update.apply(g.queue());
	return RestResult::ok(queueToJson(g.queue()));
}

static RestResult deleteQueue(const RestRequest& req)
{
	const QString uuid = req.pathParams["queue"];
	const QString moveTo = req.query.value("moveTo");
	const bool deleteTransfers = req.queryBool("deleteTransfers");
	const bool withData = req.queryBool("withData");

	if (!moveTo.isEmpty() && deleteTransfers)
		throw RestError(422, "invalid_property", "moveTo and deleteTransfers are mutually exclusive");
	if (!moveTo.isEmpty() && moveTo == uuid)
		throw RestError(422, "invalid_property", "moveTo must differ from the queue being deleted");

	Queue* removed = nullptr;
	{
		QWriteLocker l(&g_queuesLock);
		int index = -1;
		Queue* target = nullptr;

		for (int i = 0; i < g_queues.size(); i++)
		{
			if (g_queues[i]->uuid() == uuid)
				index = i;
			if (!moveTo.isEmpty() && g_queues[i]->uuid() == moveTo)
				target = g_queues[i];
		}
		if (index < 0 || (!moveTo.isEmpty() && !target))
			throw queueNotFound();

		Queue* q = g_queues[index];
		q->lockW();

		try
		{
			if (q->size() > 0 && moveTo.isEmpty() && !deleteTransfers)
				throw RestError(409, "queue_not_empty", "The queue is not empty, use moveTo or deleteTransfers");

			if (target)
			{
				while (q->size() > 0)
					target->add(q->take(0, true));
			}
			else
			{
				while (q->size() > 0)
				{
					if (withData)
						q->removeWithData(0, true);
					else
						q->remove(0, true);
				}
			}
		}
		catch (...)
		{
			q->unlock();
			throw;
		}
		q->unlock();

		g_queues.removeAt(index);
		removed = q;
	}

	removed->deleteLater();
	Queue::saveQueuesAsync();
	return RestResult::noContent();
}

static RestResult moveQueue(const RestRequest& req)
{
	QJsonObject body = req.jsonObject();
	const QString uuid = RestApi::asString(body["uuid"], "uuid");
	const QString direction = RestApi::asString(body["direction"], "direction").toLower();

	if (direction != "up" && direction != "down" && direction != "top" && direction != "bottom")
		throw RestError(422, "invalid_direction", "direction must be one of top, up, down, bottom");

	{
		QWriteLocker l(&g_queuesLock);
		int index = -1;

		for (int i = 0; i < g_queues.size(); i++)
		{
			if (g_queues[i]->uuid() == uuid)
				index = i;
		}
		if (index < 0)
			throw queueNotFound();

		int to = index;
		if (direction == "top")
			to = 0;
		else if (direction == "bottom")
			to = g_queues.size() - 1;
		else if (direction == "up")
			to = qMax(0, index - 1);
		else
			to = qMin(g_queues.size() - 1, index + 1);

		g_queues.move(index, to);
	}

	Queue::saveQueuesAsync();
	return RestResult::noContent();
}

static RestResult listQueueTransfers(const RestRequest& req)
{
	QueueGuard g(req.pathParams["queue"], QueueGuard::ReadLock);
	QJsonArray rv;

	for (int i = 0; i < g.queue()->size(); i++)
		rv << transferToJson(g.queue()->at(i));
	return RestResult::ok(rv);
}

static RestResult queueSpeedData(const RestRequest& req)
{
	QueueGuard g(req.pathParams["queue"]);
	return RestResult::ok(speedDataToJson(g.queue()->speedData()));
}

static RestResult moveTransfersBefore(const QString& queueUuid, QStringList uuids, const QString& before, bool toEnd)
{
	QueueGuard g(queueUuid, QueueGuard::WriteLock);
	Queue* q = g.queue();

	if (!toEnd && uuids.contains(before))
		throw RestError(422, "invalid_property", "\"before\" must not be among the moved transfers");

	int beforePos = -1;
	QList<Transfer*> moved;
	QList<Transfer*> rest;

	for (int i = 0; i < q->size(); i++)
	{
		Transfer* t = q->at(i);
		int j = uuids.indexOf(t->uuid());

		if (j >= 0)
		{
			moved << t;
			uuids.removeAt(j);
		}
		else
			rest << t;
		if (!toEnd && t->uuid() == before)
			beforePos = rest.size() - 1;
	}

	if (!uuids.isEmpty())
		throw transferNotFound();
	if (!toEnd && beforePos < 0)
		throw RestError(422, "invalid_property", "\"before\" is not a transfer of this queue");

	// rearrange in place, so that nothing is deleted or re-added
	QList<Transfer*> order = rest;
	const int insertAt = toEnd ? order.size() : beforePos;
	for (int i = 0; i < moved.size(); i++)
		order.insert(insertAt + i, moved[i]);

	for (int i = 0; i < order.size(); i++)
	{
		for (int j = i; j < q->size(); j++)
		{
			if (q->at(j) == order[i])
			{
				if (j != i)
					q->moveToPos(j, i, true);
				break;
			}
		}
	}
	return RestResult::noContent();
}

static RestResult moveTransfers(const RestRequest& req)
{
	QJsonObject body = req.jsonObject();
	QStringList uuids = RestApi::asStringList(body["uuids"], "uuids");

	if (body.contains("before") == body.contains("direction"))
		throw RestError(422, "invalid_property", "Exactly one of \"direction\" and \"before\" is required");

	if (body.contains("before"))
	{
		QString before;
		if (!body["before"].isNull())
			before = RestApi::asString(body["before"], "before");
		return moveTransfersBefore(req.pathParams["queue"], uuids, before, body["before"].isNull());
	}

	QString direction = RestApi::asString(body["direction"], "direction").toLower();

	if (direction != "up" && direction != "down" && direction != "top" && direction != "bottom")
		throw RestError(422, "invalid_direction", "direction must be one of top, up, down, bottom");

	QueueGuard g(req.pathParams["queue"], QueueGuard::WriteLock);
	Queue* q = g.queue();

	QList<int> positions;
	for (int i = 0; i < q->size(); i++)
	{
		int j = uuids.indexOf(q->at(i)->uuid());
		if (j >= 0)
		{
			positions << i;
			uuids.removeAt(j);
		}
	}

	if (!uuids.isEmpty())
		throw transferNotFound();

	std::sort(positions.begin(), positions.end());

	if (direction == "up")
	{
		for (int pos : positions)
			q->moveUp(pos, true);
	}
	else if (direction == "bottom")
	{
		int x = 0;
		for (int pos : positions)
			q->moveToBottom(pos - x++, true);
	}
	else if (direction == "top")
	{
		for (int i = positions.size() - 1, x = 0; i >= 0; i--, x++)
			q->moveToTop(positions[i] + x, true);
	}
	else // down
	{
		for (int i = positions.size() - 1; i >= 0; i--)
			q->moveDown(positions[i], true);
	}

	return RestResult::noContent();
}

static RestResult addTransfers(const RestRequest& req)
{
	AddOptions opts;
	QString uploadedName;
	QByteArray uploadedData;

	if (req.isMultipart())
	{
		if (!req.formFiles.contains("file"))
			throw RestError(422, "missing_file", "The \"file\" part is required");

		QJsonParseError err;
		QJsonDocument doc = QJsonDocument::fromJson(req.formFields.value("options", "{}").toUtf8(), &err);
		if (err.error != QJsonParseError::NoError || !doc.isObject())
			throw RestError(400, "invalid_json", "The \"options\" part must be a JSON object");

		opts = AddOptions::parse(doc.object(), false);
		uploadedName = req.formFiles["file"].filename;
		uploadedData = req.formFiles["file"].data;
	}
	else
		opts = AddOptions::parse(req.jsonObject(), true);

	QueueGuard g(req.pathParams["queue"]);

	TransferAdder::Options add;
	add.engine = opts.engine;
	add.target = opts.target;
	add.upload = opts.upload && !req.isMultipart(); // an uploaded file is always something to download
	add.paused = opts.paused;
	add.down = opts.down;
	add.up = opts.up;

	QStringList uuids;
	try
	{
		if (req.isMultipart())
		{
			QTemporaryFile tempFile;
			if (!tempFile.open())
				throw RestError(500, "temp_file_failed", "Cannot create a temporary file.");

			tempFile.write(uploadedData);
			tempFile.flush();

			if (add.engine.isEmpty())
			{
				if (Transfer::BestEngine be = Transfer::bestEngine("/" + uploadedName, Transfer::Download))
					add.engine = be.engine->shortName;
				if (add.engine.isEmpty())
					throw RestError(422, "no_matching_engine", "No download class is able to handle this file.");
			}
			uuids = TransferAdder::add(g.queue(), { tempFile.fileName() }, add);
		}
		else
			uuids = TransferAdder::add(g.queue(), opts.urls, add);
	}
	catch (const TransferAdder::Error& e)
	{
		throw RestError(e.code() == "create_failed" ? 500 : 422, e.code(), e.what());
	}

	QJsonObject rv;
	rv["uuids"] = QJsonArray::fromStringList(uuids);
	if (uuids.size() == 1)
		return RestResult::created(rv, QString("%1/transfers/%2").arg(RestApi::Prefix, uuids[0]));

	RestResult r = RestResult::ok(rv);
	r.status = 201;
	return r;
}

static RestResult getTransfer(const RestRequest& req)
{
	TransferGuard g(req.pathParams["transfer"]);
	return RestResult::ok(transferToJson(g.transfer()));
}

static RestResult getTransferDetails(const RestRequest& req)
{
	TransferGuard g(req.pathParams["transfer"]);
	QVariantMap vmap;

	if (TransferHttpService* s = dynamic_cast<TransferHttpService*>(g.transfer()))
		vmap = s->properties();
	return RestResult::ok(QJsonObject::fromVariantMap(vmap));
}

// The form as the class declares it. A class's own form is never invalid; the
// assertion catches a mistake in one while developing.
static QJsonObject checkedOptionsForm(Transfer* t)
{
	QJsonObject form = t->optionsForm();
	Q_ASSERT_X(FormValidator::checkForm(form).isEmpty(), "optionsForm",
		qPrintable(t->myClass() + ": " + FormValidator::checkForm(form)));
	return form;
}

// Both options handlers run on the main thread: the forms read settings arrays (the proxy list), and
// setOptions can rename files or change the state of a transfer.
static RestResult getTransferOptions(const RestRequest& req)
{
	QJsonObject form;
	RestResources::onMainThread([&] {
		TransferGuard g(req.pathParams["transfer"]);
		form = checkedOptionsForm(g.transfer());
	});
	return RestResult::ok(form);
}

static RestResult patchTransferOptions(const RestRequest& req)
{
	QJsonObject body = req.jsonObject();

	QJsonObject result;
	RestResources::onMainThread([&] {
		TransferGuard g(req.pathParams["transfer"]);
		QString error = FormValidator::checkValues(checkedOptionsForm(g.transfer()), body);

		if (!error.isEmpty())
			throw RestError(422, "invalid_property", "Invalid option: " + error);

		try
		{
			g.transfer()->setOptions(body);
		}
		catch (const RuntimeException& e)
		{
			throw RestError(422, "invalid_property", e.what());
		}
		result = checkedOptionsForm(g.transfer());
	});

	Queue::saveQueuesAsync();
	return RestResult::ok(result);
}

static RestResult transferSpeedData(const RestRequest& req)
{
	TransferGuard g(req.pathParams["transfer"]);
	return RestResult::ok(speedDataToJson(g.transfer()->speedData()));
}

// Moves the transfers to the end of the destination queue, in the given order. The caller must have checked
// that all the UUIDs and the queue exist.
static void moveTransfersToQueue(const QStringList& uuids, const QString& queueUuid)
{
	QReadLocker l(&g_queuesLock);
	Queue* dest = nullptr;

	for (Queue* q : g_queues)
	{
		if (q->uuid() == queueUuid)
			dest = q;
	}
	if (!dest)
		throw queueNotFound();

	QList<Transfer*> moved;
	for (const QString& uuid : uuids)
	{
		for (Queue* q : g_queues)
		{
			q->lockW();
			bool found = false;

			for (int i = 0; i < q->size(); i++)
			{
				if (q->at(i)->uuid() == uuid)
				{
					moved << q->take(i, true);
					found = true;
					break;
				}
			}
			q->unlock();
			if (found)
				break;
		}
	}
	dest->add(moved);
}

static void updateTransfers(const QStringList& uuids, const QJsonObject& properties)
{
	TransferUpdate update = TransferUpdate::parse(properties);

	// all-or-nothing as far as bad input is concerned: check every transfer before touching any.
	// An I/O failure in setObject during a bulk update can still leave earlier transfers changed.
	for (const QString& uuid : uuids)
	{
		TransferGuard g(uuid);
		update.check(g.transfer());
	}
	if (update.queue)
		QueueGuard g(*update.queue);

	bool changed = false;
	try
	{
		for (const QString& uuid : uuids)
		{
			TransferGuard g(uuid);
			changed = true; // apply() may have changed something even if it throws
			update.apply(g.transfer());
		}

		if (update.queue)
			moveTransfersToQueue(uuids, *update.queue);
	}
	catch (...)
	{
		// keep what was applied before the failure
		if (changed)
			Queue::saveQueuesAsync();
		throw;
	}

	Queue::saveQueuesAsync();
}

static RestResult patchTransfer(const RestRequest& req)
{
	updateTransfers(QStringList() << req.pathParams["transfer"], req.jsonObject());
	return RestResult::noContent();
}

static RestResult patchTransfers(const RestRequest& req)
{
	QJsonObject body = req.jsonObject();
	QStringList uuids = RestApi::asStringList(body["uuids"], "uuids");

	if (!body["properties"].isObject())
		throw RestError(422, "invalid_property", "Property \"properties\" must be an object");

	updateTransfers(uuids, body["properties"].toObject());
	return RestResult::noContent();
}

static void deleteTransfers(const QStringList& uuids, bool withData)
{
	for (const QString& uuid : uuids)
		TransferGuard g(uuid);

	for (const QString& uuid : uuids)
	{
		TransferGuard g(uuid, true);
		if (withData)
			g.queue()->removeWithData(g.position(), true);
		else
			g.queue()->remove(g.position(), true);
	}
}

static RestResult deleteTransfer(const RestRequest& req)
{
	deleteTransfers(QStringList() << req.pathParams["transfer"], req.queryBool("withData"));
	return RestResult::noContent();
}

static RestResult deleteTransferCollection(const RestRequest& req)
{
	QStringList uuids = req.queryValues("uuid");
	if (uuids.isEmpty())
		throw RestError(422, "missing_parameter", "At least one \"uuid\" query parameter is required");

	deleteTransfers(uuids, req.queryBool("withData"));
	return RestResult::noContent();
}

static RestResult getSettings(const RestRequest& req)
{
	QStringList keys = req.queryValues("key");
	if (keys.isEmpty())
		throw RestError(422, "missing_parameter", "At least one \"key\" query parameter is required");

	QJsonObject rv;
	for (const QString& key : keys)
		rv[key] = RestApi::variantToJson(getSettingsValue(key));
	return RestResult::ok(rv);
}

static RestResult patchSettings(const RestRequest& req)
{
	QJsonObject body = req.jsonObject();

	for (auto it = body.begin(); it != body.end(); ++it)
	{
		if (it.value().isNull() || it.value().isUndefined())
			throw RestError(422, "invalid_property", QString("Setting \"%1\" must not be null").arg(it.key()));
	}
	for (auto it = body.begin(); it != body.end(); ++it)
		setSettingsValue(it.key(), RestApi::jsonToVariant(it.value()));
	return RestResult::noContent();
}

static RestResult applySettings(const RestRequest&)
{
	QMetaObject::invokeMethod(QCoreApplication::instance(), []{ ::applyAllSettings(); }, Qt::QueuedConnection);
	return RestResult::accepted();
}

static RestResult getSettingsPages(const RestRequest&)
{
	QJsonArray rv;
	for (const SettingsItem& si : g_settingsPages)
	{
		if (!si.webSettingsScript || !si.webSettingsIconURL)
			continue;

		QJsonObject o;
		o["title"] = si.title;
		o["script"] = QString::fromUtf8(si.webSettingsScript);
		o["icon"] = QString::fromUtf8(si.webSettingsIconURL);
		rv << o;
	}
	return RestResult::ok(rv);
}

static RestResult generateCertificate(const RestRequest& req)
{
	const QString hostname = RestApi::asString(req.jsonObject()["hostname"], "hostname");
	const char* script = DATA_LOCATION "/data/genssl.sh";
	const char* config = DATA_LOCATION "/data/genssl.cnf";
	const QString failure = QObject::tr("Failed to generate a certificate, please ensure you have 'openssl' and 'sed' installed.");

	// Created with mode 0600 and a unique name; genssl.sh overwrites it
	QTemporaryFile file;
	if (!file.open())
		throw RestError(500, "certificate_failed", failure);

	QProcess prc;
	prc.start(script, QStringList() << config << hostname << file.fileName());
	prc.waitForFinished();

	if (prc.exitCode() != 0)
		throw RestError(500, "certificate_failed", failure);

	file.seek(0);

	QByteArray data = file.readAll();
	QDir::home().mkpath(USER_PROFILE_PATH "/data");

	QString path = QDir::homePath() + QLatin1String(USER_PROFILE_PATH) + "/data/fatrat-webui.pem";
	QFile out(path);

	if (!out.open(QIODevice::WriteOnly))
		throw RestError(500, "certificate_failed", QObject::tr("Failed to open %1 for writing.").arg(path));

	out.write(data);
	out.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
	out.close();

	QJsonObject rv;
	rv["path"] = path;
	return RestResult::ok(rv);
}

static RestResult listDirectories(const RestRequest& req)
{
	const QString queueUuid = req.query.value("queue");
	QString fallback = QDir::homePath();

	if (!queueUuid.isEmpty() && req.query.value("path").isEmpty())
	{
		QueueGuard g(queueUuid, QueueGuard::ReadLock);
		const QString dir = g.queue()->defaultDirectory();
		if (!dir.isEmpty() && QFileInfo(dir).isDir())
			fallback = dir;
	}

	return RestResult::ok(DirectoryListing::list(req.query.value("path"), fallback, req.queryBool("showHidden")));
}

void RestResources::onMainThread(const std::function<void()>& fn)
{
	QCoreApplication* app = QCoreApplication::instance();

	if (QThread::currentThread() == app->thread())
	{
		fn();
		return;
	}

	std::exception_ptr failure;
	QMetaObject::invokeMethod(app, [&] {
		try { fn(); }
		catch (...) { failure = std::current_exception(); }
	}, Qt::BlockingQueuedConnection);

	if (failure)
		std::rethrow_exception(failure);
}

void RestResources::registerAll()
{
	RestApi::registerRoute("GET", "/transfer-classes", getTransferClasses);
	RestApi::registerRoute("GET", "/filesystem/directories", listDirectories);

	RestApi::registerRoute("GET", "/queues", listQueues);
	RestApi::registerRoute("GET", "/stats", getStats);
	RestApi::registerRoute("POST", "/queues", createQueue);
	RestApi::registerRoute("POST", "/queues/move", moveQueue);
	RestApi::registerRoute("PATCH", "/queues/:queue", patchQueue);
	RestApi::registerRoute("DELETE", "/queues/:queue", deleteQueue);
	RestApi::registerRoute("GET", "/queues/:queue/transfers", listQueueTransfers);
	RestApi::registerRoute("POST", "/queues/:queue/transfers", addTransfers, true);
	RestApi::registerRoute("POST", "/queues/:queue/transfers/move", moveTransfers);
	RestApi::registerRoute("GET", "/queues/:queue/speed-data", queueSpeedData);

	RestApi::registerRoute("PATCH", "/transfers", patchTransfers);
	RestApi::registerRoute("DELETE", "/transfers", deleteTransferCollection);
	RestApi::registerRoute("GET", "/transfers/:transfer", getTransfer);
	RestApi::registerRoute("PATCH", "/transfers/:transfer", patchTransfer);
	RestApi::registerRoute("DELETE", "/transfers/:transfer", deleteTransfer);
	RestApi::registerRoute("GET", "/transfers/:transfer/details", getTransferDetails);
	RestApi::registerRoute("GET", "/transfers/:transfer/options", getTransferOptions);
	RestApi::registerRoute("PATCH", "/transfers/:transfer/options", patchTransferOptions);
	RestApi::registerRoute("GET", "/transfers/:transfer/speed-data", transferSpeedData);

	RestApi::registerRoute("GET", "/settings", getSettings);
	RestApi::registerRoute("PATCH", "/settings", patchSettings);
	RestApi::registerRoute("POST", "/settings/apply", applySettings);
	RestApi::registerRoute("GET", "/settings/pages", getSettingsPages);
	RestApi::registerRoute("POST", "/settings/web-interface/certificate", generateCertificate);

	RestTasks::setTransferVisitor([](const QString& uuid, const RestTasks::TransferFn& fn) {
		onMainThread([&] {
			TransferGuard guard(uuid);
			fn(guard.transfer(), guard.transfer()->name());
		});
	});
	registerTasks();
	registerRss();
#ifdef WITH_PYPLUGINS
	registerPlugins();
#endif
	registerScheduler();
}
