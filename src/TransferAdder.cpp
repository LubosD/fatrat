/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2026 Lubos Dolezel <lubos a dolezel.info>

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

#include "TransferAdder.h"
#include "Queue.h"
#include "TransferFactory.h"
#include <QDir>

QStringList TransferAdder::add(Queue* q, const QStringList& sources, Options opts, QStringList* rejected)
{
	const Transfer::Mode mode = opts.upload ? Transfer::Upload : Transfer::Download;

	// A download without a target goes where the queue's downloads go. An empty target would make the
	// engines write relative to FatRat's working directory (and a .torrent fetched over HTTP could then
	// not be loaded, as TorrentDownload only recognizes absolute paths as local files).
	if (mode == Transfer::Download && opts.target.isEmpty())
	{
		opts.target = q->defaultDirectory();
		if (opts.target.isEmpty())
			opts.target = QDir::homePath();
	}

	int explicitClass = -1;
	if (!opts.engine.isEmpty())
	{
		explicitClass = Transfer::getEngineID(opts.engine, mode);
		if (explicitClass < 0)
			throw Error("no_matching_engine", QString("Unknown transfer class: %1").arg(opts.engine));
	}

	QStringList uuids;
	QList<Transfer*> created;

	try
	{
		// With an explicit class every source uses it; otherwise each source is detected on its own
		// (a batch may mix e.g. HTTP links and magnet links).
		for (const QString& source : sources)
		{
			int classID = explicitClass;
			if (classID < 0)
			{
				Transfer::BestEngine eng = (mode == Transfer::Download)
					? Transfer::bestEngine(source, Transfer::Download)
					: Transfer::bestEngine(opts.target, Transfer::Upload);

				if (eng.nClass < 0)
				{
					if (rejected)
					{
						*rejected << source;
						continue;
					}
					throw Error("no_matching_engine", QString("Couldn't autodetect transfer type for \"%1\"").arg(source));
				}
				classID = eng.nClass;
			}

			Transfer* t = TransferFactory::instance()->createInstance(Transfer::getEngineName(classID, mode));
			if (!t)
				throw Error("create_failed", "Failed to create a class instance.");
			created << t;

			TransferFactory::instance()->init(t, source.trimmed(), opts.target);
			t->setUserSpeedLimits(opts.down, opts.up);
			TransferFactory::instance()->setState(t, opts.paused ? Transfer::Paused : Transfer::Waiting);
			uuids << t->uuid();
		}
	}
	catch (const Error&)
	{
		qDeleteAll(created);
		throw;
	}
	catch (const RuntimeException& e)
	{
		qDeleteAll(created);
		throw Error("transfer_failed", e.what());
	}
	catch (...)
	{
		qDeleteAll(created);
		throw;
	}

	q->lockW();
	for (Transfer* t : created)
		q->add(t);
	q->unlock();

	return uuids;
}
