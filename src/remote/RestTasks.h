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

#ifndef RESTTASKS_H
#define RESTTASKS_H
#include <QString>
#include <functional>

class Transfer;

/* /transfers/:transfer/actions and /tasks (registered by RestResources::registerTasks()). */
namespace RestTasks
{
	// Runs fn on the main thread with the transfer and its name, its queue locked
	// for reading; throws RestError(404, "transfer_not_found") if there is no such
	// transfer. A seam, so that the routes can be tested without queues:
	// RestResources::registerAll() installs the real one.
	typedef std::function<void(Transfer*, const QString& name)> TransferFn;
	typedef std::function<void(const QString& uuid, const TransferFn& fn)> TransferVisitor;

	void setTransferVisitor(const TransferVisitor& visitor);
}

#endif
